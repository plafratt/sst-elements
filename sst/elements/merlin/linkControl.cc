// Copyright 2013-2014 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2014, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include <sst_config.h>
#include <sst/core/serialization.h>

#include "merlin.h"
#include "linkControl.h"

using namespace SST;
using namespace Merlin;


LinkControl::LinkControl(Params &params) :
    rtr_link(NULL), output_timing(NULL),
    num_vns(0), id(-1),
    input_buf(NULL), output_buf(NULL),
    rtr_credits(NULL), in_ret_credits(NULL),
    curr_out_vn(0), waiting(true), have_packets(false), start_block(0),
    receiveFunctor(NULL), sendFunctor(NULL),
    parent(NULL), network_initialized(false)
{
}
    
void
LinkControl::configureLink(Component* rif, std::string port_name, const UnitAlgebra& link_bw_in,
                           int vns, const UnitAlgebra& in_buf_size,
                           const UnitAlgebra& out_buf_size)
{
    num_vns = vns;
    parent = rif;
    link_bw = link_bw_in;
    if ( link_bw.hasUnits("B/s") ) {
        link_bw *= UnitAlgebra("8b/B");
    }
    
    // Input and output buffers
    input_buf = new network_queue_t[vns];
    output_buf = new network_queue_t[vns];

    // Initialize credit arrays.  Credits are in flits, and we don't
    // yet know the flit size, so can't initialize in_ret_credits and
    // outbuf_credits yet.  Will initialize them after we get the
    // flit_size
    rtr_credits = new int[vns];
    in_ret_credits = new int[vns];
    outbuf_credits = new int[vns];

    inbuf_size = in_buf_size;
    if ( !inbuf_size.hasUnits("b") && !inbuf_size.hasUnits("B") ) {
        merlin_abort.fatal(CALL_INFO,-1,"in_buf_size must be specified in either "
                           "bits or bytes: %s\n",inbuf_size.toStringBestSI().c_str());
    }
    if ( inbuf_size.hasUnits("B") ) inbuf_size *= UnitAlgebra("8b/B");


    outbuf_size = out_buf_size;
    if ( !outbuf_size.hasUnits("b") && !outbuf_size.hasUnits("B") ) {
        merlin_abort.fatal(CALL_INFO,-1,"out_buf_size must be specified in either "
                           "bits or bytes: %s\n",outbuf_size.toStringBestSI().c_str());
    }
    if ( outbuf_size.hasUnits("B") ) outbuf_size *= UnitAlgebra("8b/B");
    
    // The output credits are set to zero and the other side of the
    // link will send the number of tokens.
    for ( int i = 0; i < vns; i++ ) rtr_credits[i] = 0;

    // Configure the links
    // For now give it a fake timebase.  Will give it the real timebase during init
    // rtr_link = rif->configureLink(port_name, time_base, new Event::Handler<LinkControl>(this,&LinkControl::handle_input));
    rtr_link = rif->configureLink(port_name, "1GHz", new Event::Handler<LinkControl>(this,&LinkControl::handle_input));
    // output_timing = rif->configureSelfLink(port_name + "_output_timing", time_base,
    //         new Event::Handler<LinkControl>(this,&LinkControl::handle_output));
    output_timing = rif->configureSelfLink(port_name + "_output_timing", "1GHz",
            new Event::Handler<LinkControl>(this,&LinkControl::handle_output));

    // Register statistics
    // packet_latency = rif->registerStatistic(new HistogramStatistic<uint32_t, uint32_t>(rif, "packet_latency", 0, 10000, 250));
    packet_latency = rif->registerStatistic<uint64_t>("packet_latency");
    send_bit_count = rif->registerStatistic<uint64_t>("send_bit_count");
    output_port_stalls = rif->registerStatistic<uint64_t>("output_port_stalls");
    
}


LinkControl::~LinkControl()
{
    delete [] input_buf;
    delete [] output_buf;
    delete [] rtr_credits;
    delete [] in_ret_credits;
    delete [] outbuf_credits;
}

void LinkControl::setup()
{
    while ( init_events.size() ) {
        delete init_events.front();
        init_events.pop_front();
    }
}

// void LinkControl::init(unsigned int phase)
// {
//     if ( phase == 0 ) {
//         // Need to send the available credits to the other side
//         for ( int i = 0; i < num_vns; i++ ) {
//             rtr_link->sendInitData(new credit_event(i,in_ret_credits[i]));
//             in_ret_credits[i] = 0;
//         }
//     }
//     // Need to recv the credits send from the other side
//     Event* ev;
//     while ( ( ev = rtr_link->recvInitData() ) != NULL ) {
//         credit_event* ce = dynamic_cast<credit_event*>(ev);
//         if ( ce != NULL ) {
//             rtr_credits[ce->vc] += ce->credits;
//             delete ev;
//         } else {
//             init_events.push_back(ev);
//         }
//     }
// }

void LinkControl::init(unsigned int phase)
{
    Event* ev;
    RtrInitEvent* init_ev;
    switch ( phase ) {
    case 0:
        {
            // Negotiate link speed.  We will take the min of the two link speeds
            init_ev = new RtrInitEvent();
            init_ev->command = RtrInitEvent::REPORT_BW;
            init_ev->ua_value = link_bw;
            rtr_link->sendInitData(init_ev);

            // In phase zero, send the number of VNs
            RtrInitEvent* ev = new RtrInitEvent();
            ev->command = RtrInitEvent::REQUEST_VNS;
            ev->int_value = num_vns;
            rtr_link->sendInitData(ev);
        }
        break;
    case 1:
        {
        // Get the link speed from the other side.  Actual link speed
        // will be the minumum the two sides
        ev = rtr_link->recvInitData();
        init_ev = dynamic_cast<RtrInitEvent*>(ev);
        if ( link_bw > init_ev->ua_value ) link_bw = init_ev->ua_value;

        // Get the flit size from the router
        ev = rtr_link->recvInitData();
        init_ev = dynamic_cast<RtrInitEvent*>(ev);
        UnitAlgebra flit_size_ua = init_ev->ua_value;
        flit_size = flit_size_ua.getRoundedValue();
        
        // Need to reset the time base of the output link
        UnitAlgebra link_clock = link_bw / flit_size_ua;

        // Need to initial the credit arrays
        for ( int i = 0; i < num_vns; i++ ) {
            in_ret_credits[i] = (inbuf_size / flit_size_ua).getRoundedValue();
            outbuf_credits[i] = (outbuf_size / flit_size_ua).getRoundedValue();
        }
        
        // std::cout << link_clock.toStringBestSI() << std::endl;
        
        TimeConverter* tc = parent->getTimeConverter(link_clock);
        output_timing->setDefaultTimeBase(tc);
        
        // Initialize links
        // Receive the endpoint ID from PortControl
        ev = rtr_link->recvInitData();
        if ( ev == NULL ) {
            // fail
        }
        if ( static_cast<BaseRtrEvent*>(ev)->getType() != BaseRtrEvent::INITIALIZATION ) {
            // fail
        }
        init_ev = static_cast<RtrInitEvent*>(ev);

        if ( init_ev->command != RtrInitEvent::REPORT_ID ) {
            // fail
        }

        id = init_ev->int_value;
        
        // Need to send available credits to other side of link
        for ( int i = 0; i < num_vns; i++ ) {
            rtr_link->sendInitData(new credit_event(i,in_ret_credits[i]));
            in_ret_credits[i] = 0;
        }
        network_initialized = true;
        }
        break;
    default:
        // For all other phases, look for credit events, any other
        // events get passed up to containing component by adding them
        // to init_events queue
        while ( ( ev = rtr_link->recvInitData() ) != NULL ) {
            credit_event* ce = dynamic_cast<credit_event*>(ev);
            if ( ce != NULL ) {
                if ( ce-> vc < num_vns ) {  // Ignore credit events for VNs I don't have
                    rtr_credits[ce->vc] += ce->credits;
                    // std::cout << "INIT: recieved credits: " << ce->credits << ", now have " << rtr_credits[ce->vc] << std::endl;
                }
                delete ev;
            } else {
                init_events.push_back(ev);
            }
        }
        break;
    }
}


void LinkControl::finish(void)
{

}


// Returns true if there is space in the output buffer and false
// otherwise.
bool LinkControl::send(RtrEvent* ev, int vn) {
    int flits = (ev->size_in_bits + (flit_size - 1)) / flit_size;
    ev->setSizeInFlits(flits);

    if ( outbuf_credits[vn] < flits ) return false;

    outbuf_credits[vn] -= flits;
    ev->vn = vn;

    output_buf[vn].push(ev);
    if ( waiting && !have_packets ) {
        output_timing->send(1,NULL);
        waiting = false;
    }

    ev->setInjectionTime(parent->getCurrentSimTimeNano());

    if ( ev->getTraceType() != RtrEvent::NONE ) {
        std::cout << "TRACE(" << ev->getTraceID() << "): " << parent->getCurrentSimTimeNano()
                  << " ns: Sent on LinkControl in NIC: "
                  << parent->getName() << std::endl;
    }
    return true;
}


// Returns true if there is space in the output buffer and false
// otherwise.
bool LinkControl::spaceToSend(int vn, int bits) {
    if ( (outbuf_credits[vn] * flit_size) < bits) return false;
    return true;
}


// Returns NULL if no event in input_buf[vn]. Otherwise, returns
// the next event.
RtrEvent* LinkControl::recv(int vn) {
    if ( input_buf[vn].size() == 0 ) return NULL;

    RtrEvent* event = input_buf[vn].front();
    input_buf[vn].pop();

    // Figure out how many credits to return
    int flits = event->getSizeInFlits();
    in_ret_credits[vn] += flits;

    // For now, we're just going to send the credits back to the
    // other side.  The required BW to do this will not be taken
    // into account.
    rtr_link->send(1,new credit_event(vn,in_ret_credits[vn]));
    in_ret_credits[vn] = 0;

    if ( event->getTraceType() != RtrEvent::NONE ) {
        std::cout << "TRACE(" << event->getTraceID() << "): " << parent->getCurrentSimTimeNano()
                  << " ns: recv called on LinkControl in NIC: "
                  << parent->getName() << std::endl;
    }

    return event;
}

void LinkControl::sendInitData(RtrEvent *ev)
{
    rtr_link->sendInitData(ev);
}

Event* LinkControl::recvInitData()
{
    if ( init_events.size() ) {
        Event *ev = init_events.front();
        init_events.pop_front();
        return ev;
    } else {
        return NULL;
    }
}



void LinkControl::handle_input(Event* ev)
{
    // Check to see if this is a credit or data packet
    // credit_event* ce = dynamic_cast<credit_event*>(ev);
    // if ( ce != NULL ) {
    BaseRtrEvent* base_event = static_cast<BaseRtrEvent*>(ev);
    if ( base_event->getType() == BaseRtrEvent::CREDIT ) {
    	credit_event* ce = static_cast<credit_event*>(ev);
        rtr_credits[ce->vc] += ce->credits;
        // std::cout << "Got " << ce->credits << " credits for VN: " << ce->vc << ".  Current credits: " << rtr_credits[ce->vc] << std::endl;
        delete ev;

        // If we're waiting, we need to send a wakeup event to the
        // output queues
        if ( waiting ) {
            output_timing->send(1,NULL);
            waiting = false;
            // If we were stalled waiting for credits and we had
            // packets, we need to add stall time
            if ( have_packets) {
                output_port_stalls->addData(Simulation::getSimulation()->getCurrentSimCycle() - start_block);
            }
        }
    }
    else {
        // std::cout << "LinkControl received an event" << std::endl;
        RtrEvent* event = static_cast<RtrEvent*>(ev);
        // Simply put the event into the right virtual network queue
        input_buf[event->vn].push(event);
        if ( event->getTraceType() == RtrEvent::FULL ) {
            std::cout << "TRACE(" << event->getTraceID() << "): " << parent->getCurrentSimTimeNano()
                      << " ns: Received an event on LinkControl in NIC: "
                      << parent->getName() << " on VN " << event->vn << " from src " << event->src
                      << "." << std::endl;
        }
        if ( receiveFunctor != NULL ) {
            bool keep = (*receiveFunctor)(event->vn);
            if ( !keep) receiveFunctor = NULL;
        }
        SimTime_t lat = parent->getCurrentSimTimeNano() - event->getInjectionTime();
        packet_latency->addData(lat);
        stats.insertPacketLatency(lat);
    }
}


void LinkControl::handle_output(Event* ev)
{
    // The event is an empty event used just for timing.

    // ***** Need to add in logic for when to return credits *****
    // For now just done automatically when events are pulled out
    // of the block

    // We do a round robin scheduling.  If the current vn has no
    // data, find one that does.
    int vn_to_send = -1;
    bool found = false;
    RtrEvent* send_event = NULL;
    have_packets = false;

    for ( int i = curr_out_vn; i < num_vns; i++ ) {
        if ( output_buf[i].empty() ) continue;
        have_packets = true;
        send_event = output_buf[i].front();
        // Check to see if the needed VN has enough space
        if ( rtr_credits[i] < send_event->getSizeInFlits() ) continue;
        vn_to_send = i;
        output_buf[i].pop();
        found = true;
        break;
    }

    if (!found)  {
        for ( int i = 0; i < curr_out_vn; i++ ) {
            if ( output_buf[i].empty() ) continue;
            have_packets = true;
            send_event = output_buf[i].front();
            // Check to see if the needed VN has enough space
            if ( rtr_credits[i] < send_event->getSizeInFlits() ) continue;
            vn_to_send = i;
            output_buf[i].pop();
            found = true;
            break;
        }
    }


    // If we found an event to send, go ahead and send it
    if ( found ) {
        // Send the output to the network.
        // First set the virtual channel.
        send_event->vn = vn_to_send;

        // Need to return credits to the output buffer
        int size = send_event->getSizeInFlits();
        outbuf_credits[vn_to_send] += size;

        // Send an event to wake up again after this packet is sent.
        output_timing->send(size,NULL);

        curr_out_vn = vn_to_send + 1;
        if ( curr_out_vn == num_vns ) curr_out_vn = 0;

        // Add in inject time so we can track latencies
        send_event->setInjectionTime(parent->getCurrentSimTimeNano());
        
        // Subtract credits
        rtr_credits[vn_to_send] -= size;
        rtr_link->send(send_event);
        // std::cout << "Sent packet on vn " << vn_to_send << ", credits remaining: " << rtr_credits[vn_to_send] << std::endl;
        
        if ( send_event->getTraceType() == RtrEvent::FULL ) {
            std::cout << "TRACE(" << send_event->getTraceID() << "): " << parent->getCurrentSimTimeNano()
                      << " ns: Sent an event to router from LinkControl in NIC: "
                      << parent->getName() << " on VN " << send_event->vn << " to dest " << send_event->dest
                      << "." << std::endl;
        }
        send_bit_count->addData(send_event->size_in_bits);
        if (sendFunctor != NULL ) {
            bool keep = (*sendFunctor)(vn_to_send);
            if ( !keep ) sendFunctor = NULL;
        }
    }
    else {
        // What do we do if there's nothing to send??  It could be
        // because everything is empty or because there's not
        // enough room in the router buffers.  Either way, we
        // don't send a wakeup event.  We will send a wake up when
        // we either get something new in the output buffers or
        // receive credits back from the router.  However, we need
        // to know that we got to this state.
        // std::cout << "Waiting ..." << std::endl;
        start_block = Simulation::getSimulation()->getCurrentSimCycle();
        waiting = true;
    }
}


void LinkControl::PacketStats::insertPacketLatency(SimTime_t lat)
{
    numPkts++;
    if ( 1 == numPkts ) {
        minLat = lat;
        maxLat = lat;
        m_n = m_old = lat;
        s_old = 0.0;
    } else {
        minLat = std::min(minLat, lat);
        maxLat = std::max(maxLat, lat);
        m_n = m_old + (lat - m_old) / numPkts;
        s_n = s_old + (lat - m_old) * (lat - m_n);

        m_old = m_n;
        s_old = s_n;
    }
}


