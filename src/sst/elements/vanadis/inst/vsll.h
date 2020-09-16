
#ifndef _H_VANADIS_SLL
#define _H_VANADIS_SLL

#include "inst/vinst.h"

namespace SST {
namespace Vanadis {

class VanadisShiftLeftLogicalInstruction : public VanadisInstruction {
public:
	VanadisShiftLeftLogicalInstruction(
		const uint64_t id,
		const uint64_t addr,
		const uint32_t hw_thr,
		const VanadisDecoderOptions* isa_opts,
		const uint16_t dest,
		const uint16_t src_1,
		const uint16_t src_2) :
		VanadisInstruction(id, addr, hw_thr, isa_opts, 2, 1, 2, 1, 0, 0, 0, 0) {

		isa_int_regs_in[0]  = src_1;
		isa_int_regs_in[1]  = src_2;
		isa_int_regs_out[0] = dest;
	}

	virtual VanadisFunctionalUnitType getInstFuncType() const {
		return INST_INT_ARITH;
	}

	virtual const char* getInstCode() const {
		return "SLL";
	}

	virtual void printToBuffer(char* buffer, size_t buffer_size) {
                snprintf(buffer, buffer_size, "SLL     %5" PRIu16 " <- %5" PRIu16 " << %5" PRIu16 " (phys: %5" PRIu16 " <- %5" PRIu16 " << %5" PRIu16 ")",
			isa_int_regs_out[0], isa_int_regs_in[0], isa_int_regs_in[1], 
			phys_int_regs_out[0], phys_int_regs_in[0], phys_int_regs_in[1] );
        }

	virtual void execute( SST::Output* output, VanadisRegisterFile* regFile ) {
		output->verbose(CALL_INFO, 16, 0, "Execute: (addr=%p) SLL phys: out=%" PRIu16 " in=%" PRIu16 ", %" PRIu16 ", isa: out=%" PRIu16 " / in=%" PRIu16 ", %" PRIu16 "\n",
			(void*) getInstructionAddress(), phys_int_regs_out[0],
			phys_int_regs_in[0], phys_int_regs_in[1],
			isa_int_regs_out[0], isa_int_regs_in[0], isa_int_regs_in[1] );

		const uint64_t src_1 = regFile->getIntReg<uint64_t>( phys_int_regs_in[0] );
		const uint64_t src_2 = regFile->getIntReg<uint64_t>( phys_int_regs_in[1] );

		regFile->setIntReg( phys_int_regs_out[0], (src_1) << (src_2) );

		markExecuted();
	}

};

}
}

#endif
