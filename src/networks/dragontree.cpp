/*
  Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this list
  of conditions and the following disclaimer.
  Redistributions in binary form must reproduce the above copyright notice, this 
  list of conditions and the following disclaimer in the documentation and/or 
  other materials provided with the distribution.
  Neither the name of the Stanford University nor the names of its contributors 
  may be used to endorse or promote products derived from this software without 
  specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "booksim.hpp"
#include <vector>
#include <map>
#include <sstream>

#include "dragontree.hpp"
#include "random_utils.hpp"
#include "misc_utils.hpp"
#include "globals.hpp"

#define FLATFLY_INDEX   0
#define FATTREE_INDEX   1
#define NUM_SUBNETWORS  2
#define DETERMINISTIC   0
#define OBLIVIOUS       1
#define ADAPTIVE        2

DragonTree::DragonTree( const Configuration &config, const string & name ) : Network( config, name )
{
  // Delegate config parsing task to subnetworks to avoid realloc. 
  // Fields which are relevant to only FlatFLy or FatTree are prefixed
  // as such in the config file. E.g. 
  flat_fly_ptr = new FlatFlyOnChip(config, "flatfly");
  fat_tree_ptr = new FatTree(config, "fattree");

  _ComputeSize(config);

  for (int m = 0; m < _nodes; ++m){ //init output queues
    outputQs[m] = SubToVCQ();
    for (int n = 0; n < 2; ++n){
      outputQs[m][n] = VCToQ();
      for (int v = 0; v < num_vcs; ++v){
        outputQs[m][n][v] = FlitQ();
      }
    }
  }

  for (int i = 0; i < _nodes; ++i){ //Init current output network and VC per source
    currSrcToManager[i].subnet = FLATFLY_INDEX;
    currSrcToManager[i].vc = 0;
  }

  outputQs.resize(_nodes);
  currSrcToManager.resize(_nodes);
  lastSubnetOut.resize(_nodes);
  sourceToNetwork.resize(_nodes);

  flat_fly_lat = 0;
  fat_tree_lat = 0;
}

void DragonTree::RegisterRoutingFunctions() {
  gRoutingFunctionMap["dragontree_routing"] = &dragontree_routing;
}

void DragonTree::_BuildNet( const Configuration &caonfig ) {

}

void DragonTree::_ComputeSize( const Configuration &config ) {
  num_vcs = config.GetInt("num_vcs");
  _inject_route =  config.GetInt("inject_route");
}

void DragonTree::WriteFlit( Flit *f, int source )
{
  // TODO: Need to check which network we wrote in to grant credit back in
  //       using function read credit.
  assert( ( source >= 0 ) && ( source < _nodes ) );
  bool ffly_network = false;
  if(f->head){
    // odd source -> fat tree, even source -> ffly
    switch (_inject_route) {
      case DETERMINISTIC: ffly_network = (source % 2) == 0; break;
      case OBLIVIOUS: ffly_network = rand() % 2; break;
      case ADAPTIVE: ffly_network = adaptive_inject_routing(f, source); break;
    }

    if (ffly_network){
      packetMap[f->pid] = FLATFLY_INDEX;
      flat_fly_ptr->WriteFlit(f,source);
      sourceToNetwork[source] = FLATFLY_INDEX;
    } else { // fattree
      packetMap[f->pid] = FATTREE_INDEX;
      fat_tree_ptr->WriteFlit(f,source);
      sourceToNetwork[source] = FATTREE_INDEX;
    }
  } else { //not head flit
    if(packetMap[f->pid] == FLATFLY_INDEX){ // flat fly
      flat_fly_ptr->WriteFlit(f,source);
      sourceToNetwork[source] = FLATFLY_INDEX;
    } else { // fat tree
      fat_tree_ptr->WriteFlit(f,source);
      sourceToNetwork[source] = FATTREE_INDEX;
    }
  }
}

NetAndVC DragonTree::nextNetAndVC(NetAndVC currNetAndVC)
{
  if(currNetAndVC.subnet == FLATFLY_INDEX){
    ++currNetAndVC.subnet;
    return currNetAndVC;
  } else {
    currNetAndVC.subnet = FLATFLY_INDEX;
    ++currNetAndVC.vc;
    if (currNetAndVC.vc == num_vcs){
      currNetAndVC.vc = 0;
    }
    return currNetAndVC;
  }
}

// Returns output flit if available.
// Traffic manager tracks which packages have been accepted.
Flit *DragonTree::ReadFlit( int dest )
{
  assert( ( dest >= 0 ) && ( dest < _nodes ) );

  Flit * f = flat_fly_ptr->ReadFlit(dest);
  if (f){
    outputQs[dest][FLATFLY_INDEX][f->vc].push(f);
  }
  f = fat_tree_ptr->ReadFlit(dest);
  if (f){
    outputQs[dest][FATTREE_INDEX][f->vc].push(f);
  }
  Flit * toReturn;
  NetAndVC original = currSrcToManager[dest];
  NetAndVC outSrc = original; 
  while (outputQs[dest][outSrc.subnet][outSrc.vc].empty()){
    outSrc = nextNetAndVC(outSrc);
    if (outSrc.subnet == original.subnet && outSrc.vc == original.vc) break;
  }
  currSrcToManager[dest] = outSrc;
  if (!outputQs[dest][outSrc.subnet][outSrc.vc].empty()){
    toReturn = outputQs[dest][outSrc.subnet][outSrc.vc].front();
    outputQs[dest][outSrc.subnet][outSrc.vc].pop();
  } else {
    toReturn = 0;
  }
  lastSubnetOut[dest] = outSrc.subnet;
  if (toReturn){
    if (toReturn->tail){
      currSrcToManager[dest] = nextNetAndVC(outSrc);
    }
  }
  return toReturn;
}

void DragonTree::WriteCredit( Credit *c, int dest )
{
  assert( ( dest >= 0 ) && ( dest < _nodes ) );

  if (sourceToNetwork[dest] == FLATFLY_INDEX) {
    // flat fly case.
    flat_fly_ptr->WriteCredit(c, dest);
  } else {
    // fat tree case.
    fat_tree_ptr->WriteCredit(c, dest);
  }
}

Credit *DragonTree::ReadCredit( int source )
{
  assert( ( source >= 0 ) && ( source < _nodes ) );

  if (lastSubnetOut[source] == FLATFLY_INDEX) {
    // flat fly case.
    return flat_fly_ptr->ReadCredit(source);
  } else {
    // fat tree case
    return fat_tree_ptr->ReadCredit(source);    
  }
}

void DragonTree::ReadInputs()
{
  flat_fly_ptr->ReadInputs();
  fat_tree_ptr->ReadInputs();
}

void DragonTree::Evaluate()
{
  flat_fly_ptr->Evaluate();
  fat_tree_ptr->Evaluate();
}

void DragonTree::WriteOutputs()
{
  flat_fly_ptr->WriteOutputs();
  fat_tree_ptr->WriteOutputs();
}

void DragonTree::update_latency_prediction(bool ntwk, Flit *f) {
  if (ntwk == FLATFLY_INDEX) {
    flat_fly_lat = f->atime - f->ctime;
  } else {
    flat_fly_lat = f->atime - f->ctime;
  }
}

// returns: false - fat tree, true - flat fly
bool DragonTree::adaptive_inject_routing(Flit *f, int source) {
  // Do some routing logic to find out which subnetwork should best take this
  // flit from the specified source.

  if (flat_fly_lat < fat_tree_lat)
    return FLATFLY_INDEX;

  return FATTREE_INDEX;
}

// To bypass traffic manager routing function
void dragontree_routing( const Router *r, const Flit *f, int in_channel, 
      OutputSet *outputs, bool inject ) {
  xyyx_flatfly(r, f, in_channel, outputs, inject);
}


