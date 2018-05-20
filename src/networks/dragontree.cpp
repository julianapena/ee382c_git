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

DragonTree::DragonTree( const Configuration &config, const string & name ) : Network( config, name )
{
  //make a config file for flat_fly
  // Configuration flat_fly_config;
  //instantiate a flat_fly
  // flat_fly_ptr = new FlatFlyOnChip(&flat_fly_config, name);
  //make a config file for fat_tree
  // Configuration fat_tree_config;
  //instantiate a fat_tree
  // fat_tree_ptr = new FatTree(&fat_tree_config name);

  // Delegate config parsing task to subnetworks to avoid realloc. 
  // Fields which are relevant to only FlatFLy or FatTree are prefixed
  // as such in the config file. E.g. 
  flat_fly_ptr = new FlatFlyOnChip(config, "flatfly");
  fat_tree_ptr = new FatTree(config, "fattree");

  for (int m; m < _nodes; ++m){
    outputQMap[m] = FlitQ();
  }
}

void DragonTree::RegisterRoutingFunctions() {
  
}

void DragonTree::_BuildNet( const Configuration &config ) {

}

void DragonTree::_ComputeSize( const Configuration &config ) {

}

void DragonTree::WriteFlit( Flit *f, int source )
{
  // TODO: Need to check which network we wrote in to grant credit back in
  //       using function read credit.
  assert( ( source >= 0 ) && ( source < _nodes ) );
  if(f->head){
    if (true){ //replace condition with some routing logic
      packetMap[f->pid] = true;
      flat_fly_ptr->WriteFlit(f,source);
      SourceToNetworkMap[source].push(true);
    } else {
      packetMap[f->pid] = false;
      fat_tree_ptr->WriteFlit(f,source);
      SourceToNetworkMap[source].push(false);
    }
  } else { //not head flit
    if(packet_map.find(f->pid)->second){
      flat_fly_ptr->WriteFlit(f,source);
      SourceToNetworkMap[source].push(true);
    } else {
      fat_tree_ptr->WriteFlit(f,source);
      SourceToNetworkMap[source].push(false);
    }
  }
}

// Returns output flit if available.
// Traffic manager tracks which packages have been accepted.
Flit *DragonTree::ReadFlit( int dest )
{
  assert( ( dest >= 0 ) && ( dest < _nodes ) );

  FlitQ outputQ = outputQMap[dest];
  
  Flit *fat_tree_eject = fat_tree_ptr->ReadFlit(dest);
  if (fat_tree_eject != 0) {
    outputQ.push(fat_tree_eject);
  } 
  Flit *flat_fly_eject = flat_fly_ptr->ReadFlit(dest);
  if (flat_fly_eject != 0){
   outputQ.push(flat_fly_eject);
  }
  Flit *toReturn;
  if (!outputQ.empty()){
    toReturn = outputQ.front();
    outputQ.pop();
  } else {
    toReturn = 0;
  }
  return toReturn;


void DragonTree::WriteCredit( Credit *c, int dest )
{
  assert( ( dest >= 0 ) && ( dest < _nodes ) );

  // pop from queue - last read network
  bool flitLastRead = DestToNetworkMap[source].front();
  DestToNetworkMap[source].pop();

  if (flitLastRead == true) {
    // flat fly case.
    flat_fly_ptr->_eject_cred[dest]->Send(c);
  } else {
    // fat tree case.
    fat_tree_ptr->_eject_cred[dest]->Send(c);
  }
}

Credit *DragonTree::ReadCredit( int source )
{
  assert( ( source >= 0 ) && ( source < _nodes ) );

  // pop from queue - last written network
  bool flitLastWritten = SourceToNetworkMap[source].front();
  SourceToNetworkMap[source].pop();

  if (flitLastWritten == true) {
    // flat fly case.
    return flat_fly_ptr->_inject_cred[source]->Receive();
  } 

  // fat tree case
  return fat_tree_ptr->_inject_cred[source]->Receive();
}


