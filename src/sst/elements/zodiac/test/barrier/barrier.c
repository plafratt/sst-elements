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

#include <stdio.h>
#include <mpi.h>

int main(int argc, char* argv[]) {

	MPI_Init(&argc, &argv);

	int rank, npes;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &npes);

	if(rank == 0) {
		printf("SST Barrier Test (Rank Count: %d)\n", npes);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if(rank == 0) {
		printf("SST Barrier completed.\n");
	}

	MPI_Finalize();

}
