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

#include <sst/core/element.h>

#include <sst/core/configGraph.h>

#include "trig_cpu/trig_cpu.h"
#include "trig_nic/trig_nic.h"

#include <stdio.h>
#include <stdarg.h>

static Component* 
create_trig_cpu(SST::ComponentId_t id, 
                SST::Component::Params_t& params)
{
    return new trig_cpu( id, params );
}

static Component* 
create_trig_nic(SST::ComponentId_t id, 
                SST::Component::Params_t& params)
{
    return new trig_nic( id, params );
}


static string str(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start (args, format);
    vsprintf (buffer,format, args);
    va_end (args);
    string ret(buffer);
    return ret;
}


static void partition(ConfigGraph* graph, int ranks) {
    int sx, sy, sz;

        
    // Need to look through components until we find a router so we
    // can get the dimensions of the torus
    ConfigComponentMap_t& comps = graph->getComponentMap();
    for ( ConfigComponentMap_t::iterator iter = comps.begin();
                            iter != comps.end(); ++iter )
    {
	ConfigComponent* ccomp = (*iter).second;
	if ( ccomp->type == "SS_router.SS_router" ) {
	    sx = ccomp->params.find_integer("network.xDimSize");
	    sy = ccomp->params.find_integer("network.yDimSize");
	    sz = ccomp->params.find_integer("network.zDimSize");
	    if ( sx == -1 || sy == -1 || sz == -1 ) {
		printf("SS_router found without properly set network dimensions, aborting...\n");
		abort();
	    }
	    break;
	}
    }
    
    for ( ConfigComponentMap_t::iterator iter = comps.begin();
                            iter != comps.end(); ++iter )
    {
	ConfigComponent* ccomp = (*iter).second;
	int id = ccomp->params.find_integer("id");
	if ( id == -1 ) {
	    printf("Couldn't find id for component %s, aborting...\n",ccomp->name.c_str());
	    abort();
	}
	// Get the x, y and z dimensions for the given id.
        int x, y, z;

	z = id / (sx * sy);
	y = (id / sx) % sy;
	x = id % sx;

	/* Partition logic */
        int rank = id % ranks;
        if ( ranks == 2 ) {
            if ( x < sx/2 ) rank = 0;
            else rank = 1;
        }
        if ( ranks == 4 ) {
            rank = 0;
            if ( x >= sx/2 ) rank = rank | 1;
            if ( y >= sy/2 ) rank = rank | (1 << 1);
        }
        if ( ranks == 8 ) {
            rank = 0;
            if ( x >= sx/2 ) rank = rank | 1;
            if ( y >= sy/2 ) rank = rank | (1 << 1);
            if ( z >= sz/2 ) rank = rank | (1 << 2);
        }

        if ( ranks == 16 ) {
            rank = 0;
	    if ( x >= 3*(sx/4) ) rank = rank | 3;
	    else if ( x >= sx/2 && x < 3*(sx/4) ) rank = rank | 2;
	    else if ( x >= sx/4 && x < sx/2 ) rank = rank | 1;
            if ( y >= sy/2 ) rank = rank | (1 << 2);
            if ( z >= sz/2 ) rank = rank | (1 << 3);
        }

	ccomp->rank = rank;
	
    }    

}



static void generate(ConfigGraph* graph, string options) {
    // partition2(graph,ranks);
    // return;
    
    int x_count = 16;
    int y_count = 8;
    int z_count = 8;
    int radix = 8;
    int size = x_count * y_count * z_count;


    Params rtr_params;
    rtr_params["clock"] = "500Mhz";
    rtr_params["debug"] = "no";
    rtr_params["info"] = "no";
    rtr_params["iLCBLat"] = "13";
    rtr_params["oLCBLat"] = "7";
    rtr_params["routingLat"] = "3";
    rtr_params["iQLat"] = "2";

    rtr_params["OutputQSize_flits"] = "16";
    rtr_params["InputQSize_flits"] = "96";
    rtr_params["Router2NodeQSize_flits"] = "512";

    rtr_params["network.xDimSize"] = str("%d",x_count);
    rtr_params["network.yDimSize"] = str("%d",y_count);
    rtr_params["network.zDimSize"] = str("%d",z_count);

    rtr_params["routing.xDateline"] = "0";
    rtr_params["routing.yDateline"] = "0";
    rtr_params["routing.zDateline"] = "0";

    Params cpu_params;
    cpu_params["radix"] = str("%d",radix);
    cpu_params["timing_set"] = "2";
    cpu_params["nodes"] = str("%d",size);
    cpu_params["msgrate"] = "5MHz";
    cpu_params["xDimSize"] = str("%d",x_count);
    cpu_params["yDimSize"] = str("%d",y_count);
    cpu_params["zDimSize"] = str("%d",z_count);
    cpu_params["noiseRuns"] = "0";
    cpu_params["noiseInterval"] = "1kHz";
    cpu_params["noiseDuration"] = "25us";
    cpu_params["application"] = "allreduce.tree_triggered";
    cpu_params["latency"] = "500";
    cpu_params["msg_size"] = "1048576";
    cpu_params["chunk_size"] = "16384";
    cpu_params["coalesce"] = "0";
    cpu_params["enable_putv"] = "0";

    Params nic_params;
    nic_params["clock"] = "500MHz";
    nic_params["timing_set"] = "2";
    nic_params["info"] = "no";
    nic_params["debug"] = "no";
    nic_params["dummyDebug"] = "no";
    nic_params["latency"] = "500";

    string nic_link_lat = "200ns";
    string rtr_link_lat = "10ns";
    
    ComponentId_t cid;
    for ( int i = 0; i < size; ++i) {
        int x, y, z;

	z = i / (x_count * y_count);
	y = (i / x_count) % y_count;
	x = i % x_count;

	
	cid = graph->addComponent(str("%d.cpu",i), "portals4_sm.trig_cpu");
	graph->addParams(cid, cpu_params);
	graph->addParameter(cid, "id", str("%d",i));
	graph->addLink(cid, str("%d.cpu2nic",i), "nic", nic_link_lat);

	cid = graph->addComponent(str("%d.nic",i), "portals4_sm.trig_nic");
	graph->addParams(cid, nic_params);
	graph->addParameter(cid, "id", str("%d",i));
	graph->addLink(cid, str("%d.cpu2nic",i), "cpu", nic_link_lat);
	graph->addLink(cid, str("%d.nic2rtr",i), "rtr", nic_link_lat);

	cid = graph->addComponent(str("%d.rtr",i), "SS_router.SS_router");
	graph->addParams(cid, rtr_params);
	graph->addParameter(cid, "id", str("%d",i));
	graph->addLink(cid, str("%d.nic2rtr",i),"nic",nic_link_lat);

	if ( x_count > 1 ) {
	    graph->addLink(cid, str("xr2r.%d.%d.%d",y,z,((x+1)%x_count)), "xPos", rtr_link_lat);
	    graph->addLink(cid, str("xr2r.%d.%d.%d",y,z,x), "xNeg", rtr_link_lat);
	}
	
	if ( y_count > 1 ) {
	    graph->addLink(cid, str("yr2r.%d.%d.%d",x,z,((y+1)%y_count)), "yPos", rtr_link_lat);
	    graph->addLink(cid, str("yr2r.%d.%d.%d",x,z,y), "yNeg", rtr_link_lat);
	}
	
	if ( z_count > 1 ) {
	    graph->addLink(cid, str("zr2r.%d.%d.%d",x,y,((z+1)%z_count)), "zPos", rtr_link_lat);
	    graph->addLink(cid, str("zr2r.%d.%d.%d",x,y,z), "zNeg", rtr_link_lat);
	}
	
    }
}




static const ElementInfoComponent components[] = {
    { "trig_cpu",
      "Triggered CPU for Portals 4 research",
      NULL,
      create_trig_cpu,
    },
    { "trig_nic",
      "Triggered NIC for Portals 4 research",
      NULL,
      create_trig_nic,
    },
    { NULL, NULL, NULL, NULL }
};

static const ElementInfoPartitioner partitioners[] = {
    { "partitioner",
      "Partitioner for portals4_sm simulations",
      NULL,
      partition,
    },
    { NULL, NULL, NULL, NULL }
};
      
static const ElementInfoGenerator generators[] = {
    { "generator",
      "Generator for portals4_sm simulations",
      NULL,
      generate,
    },
    { NULL, NULL, NULL, NULL }
};
      
					     

extern "C" {
    ElementLibraryInfo portals4_sm_eli = {
        "portals4_sm",
        "State-machine based processor/nic for Portals 4 research",
        components,
	NULL,
	NULL,
	partitioners,
	generators,
    };
}
