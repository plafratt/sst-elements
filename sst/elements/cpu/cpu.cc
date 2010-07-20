// Copyright 2009-2010 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2010, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include <sst_config.h>
#include "sst/core/serialization/element.h"

#include "cpu.h"
#include <sst/core/timeConverter.h>
#include "myMemEvent.h"


// bool Cpu::clock( Cycle_t current, Time_t epoch )
bool Cpu::clock( Cycle_t current )
{
    //_CPU_DBG("id=%lu currentCycle=%lu \n", Id(), current );

//     printf("In CPU::clock\n");
    MyMemEvent* event = NULL; 

    if (current == 100000 ) unregisterExit();

    if ( state == SEND ) { 
        if ( ! event ) event = new MyMemEvent();

        if ( who == WHO_MEM ) { 
            event->address = 0x1000; 
            who = WHO_NIC;
        } else {
            event->address = 0x10000000; 
            who = WHO_MEM;
        }

        _CPU_DBG("xxx: send a MEM event address=%#lx @ cycle %ld\n", event->address, current );
// 	mem->Send( 3 * epoch, event );
	mem->Send( (Cycle_t)3, event );
 	std::cout << "CPU " << getId() << "::clock -> setting state to WAIT at cycle "<< current <<std::endl;
        state = WAIT;
    } else {
// 	printf("Entering state WAIT\n");
        if ( ( event = static_cast< MyMemEvent* >( mem->Recv() ) ) ) {
// 	    printf("Got a mem event\n");
	  _CPU_DBG("xxx: got a MEM event address=%#lx @ cycle %ld\n", event->address, current );
	std::cout << "CPU " << getId() << "::clock -> setting state to SEND at cycle "<< current <<std::endl;
            state = SEND;
        }
    }
    return false;
}

extern "C" {
Cpu*
cpuAllocComponent( SST::ComponentId_t id, 
                   SST::Component::Params_t& params )
{
//     printf("cpuAllocComponent--> sim = %p\n",sim);
    return new Cpu( id, params );
}
}

BOOST_CLASS_EXPORT(Cpu)
BOOST_CLASS_EXPORT(SST::MyMemEvent)
