/*
 Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this 
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

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
                                                                     
                                                                     
                                             
#ifndef _DRAGONTREE_HPP_
#define _DRAGONTREE_HPP_

#include <map>
#include <queue>

#include "network.hpp"
#include "flatfly_onchip.hpp"
#include "fattree.hpp"
#include "routefunc.hpp"

// bool: false - fat tree, true - flat fly

struct NetAndVC{
  int subnet;
  int vc;
};

typedef std::map<int, int> PacketToSubnetworkMap;
typedef std::queue<Flit *> FlitQ;
typedef std::vector<FlitQ> VCToQ;
typedef std::vector<VCToQ> SubToVCQ;
typedef std::vector<SubToVCQ> NodeToSubToVCQ;
typedef std::vector<NetAndVC> CurrentOutputSource;
typedef std::vector<int> LastOutputSource;
typedef std::vector<int> NodeToNetwork;

class DragonTree : public Network {

  int num_vcs;
  int _inject_route;
 
  Network *flat_fly_ptr;
  Network *fat_tree_ptr;
  PacketToSubnetworkMap packetMap;
  NodeToSubToVCQ outputQs;
  CurrentOutputSource currSrcToManager;
  LastOutputSource lastSubnetOut;
  NodeToNetwork sourceToNetwork;

  // keep track of latency foor adaptive routing.
  int flat_fly_lat;
  int fat_tree_lat;

  NetAndVC nextNetAndVC(NetAndVC currNetAndVC);

  void _ComputeSize( const Configuration &config );
  void _BuildNet( const Configuration &config );
  bool adaptive_inject_routing(Flit *f, int source);  
  void update_latency_prediction(bool ntwk, Flit *f);

public:
  DragonTree( const Configuration &config, const string & name );

  void WriteFlit( Flit *f, int source );
  Flit *ReadFlit( int dest );

  void    WriteCredit( Credit *c, int dest );
  Credit *ReadCredit( int source );

  void ReadInputs( );
  void Evaluate( );
  void WriteOutputs( );

  static void RegisterRoutingFunctions();
};


void dragontree_routing( const Router *r, const Flit *f, int in_channel, 
      OutputSet *outputs, bool inject );

#endif 
