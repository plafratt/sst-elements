// Copyright 2009-2019 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2019, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _H_EMBER_FAM_SCATTER_EVENT
#define _H_EMBER_FAM_SCATTER_EVENT

#include "emberFamEvent.h"

namespace SST {
namespace Ember {

class EmberFamScatter_Event : public EmberFamEvent {

public:
	EmberFamScatter_Event( Shmem::Interface& api, Output* output,
			Hermes::Vaddr src, Shmem::Fam_Descriptor fd, uint64_t nblocks, uint64_t firstBlock, uint64_t stride, 
			uint64_t blockSize, bool blocking, EmberEventTimeStatistic* stat = NULL ) :
			EmberFamEvent( api, output, stat ), m_src(src), m_fd(fd), m_nblocks(nblocks), m_firstBlock(firstBlock),
			m_stride(stride), m_blockSize(blockSize), m_blocking(blocking) {}

	~EmberFamScatter_Event() {}

    std::string getName() { return "FamScatter"; }

    void issue( uint64_t time, Shmem::Callback callback ) {

        EmberEvent::issue( time );
        m_api.fam_scatter( m_src, m_fd, m_nblocks, m_firstBlock, m_stride, m_blockSize, m_blocking, callback );
    }
private:
    Hermes::Vaddr m_src;
	Shmem::Fam_Descriptor m_fd;
	uint64_t m_nblocks;
	uint64_t m_firstBlock;
	uint64_t m_stride;
	uint64_t m_blockSize;
	bool m_blocking;
};

}
}

#endif
