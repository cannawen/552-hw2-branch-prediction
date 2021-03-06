/* sim-safe.c - sample functional simulator implementation */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"

/*
 * This file implements a functional simulator.  This functional simulator is
 * the simplest, most user-friendly simulator in the simplescalar tool set.
 * Unlike sim-fast, this functional simulator checks for all instruction
 * errors, and the implementation is crafted for clarity rather than speed.
 */


/* ECE552 Assignment 2 - STATS COUNTERS - BEGIN */ 

static counter_t sim_num_br = 0;						//total number of conditional branches
static counter_t sim_num_mispred_static = 0;	//number of mispredicted branches of the static predict-taken predictor
static counter_t sim_num_mispred_random = 0;	//number of mispredicted branches of the random predictor
static counter_t sim_num_mispred_2bitsat = 0;	//number of mispredicted branches of the 2-bit saturating counter predictor
static counter_t sim_num_mispred_2level = 0;	//number of mispredicted branches of two level predictor
static counter_t sim_num_mispred_openend = 0;	//number of mispredicted branches of the open-ended predictor

/* ECE552 Assignment 2 - STATS COUNTERS - END */
 
/* ECE552 Assignment 2 - BEGIN CODE*/ 

int twobit[4096];
int GHR;
int twolvl[8][512];
#define OTABLES 128
#define OTABLESBITS 0x7F
#define OLINES 512
#define OLINESBITS 0x1FF
int OGHR;
int open[OTABLES][OLINES];

/* ECE552 Assignment 2 - END CODE*/ 

 
/* simulated registers */
static struct regs_t regs;

/* simulated memory */
static struct mem_t *mem = NULL;

/* track number of refs */
static counter_t sim_num_refs = 0;

/* maximum number of inst's to execute */
static unsigned int max_insts;

/* register simulator-specific options */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
"sim-safe: This simulator implements a functional simulator.  This\n"
"functional simulator is the simplest, most user-friendly simulator in the\n"
"simplescalar tool set.  Unlike sim-fast, this functional simulator checks\n"
"for all instruction errors, and the implementation is crafted for clarity\n"
"rather than speed.\n"
		 );

  /* instruction limit */
  opt_reg_uint(odb, "-max:inst", "maximum number of inst's to execute",
	       &max_insts, /* default */0,
	       /* print */TRUE, /* format */NULL);

}

/* check simulator-specific option values */
void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  /* nada */
}

/* register simulator-specific statistics */
void
sim_reg_stats(struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, "sim_num_insn",
		   "total number of instructions executed",
		   &sim_num_insn, sim_num_insn, NULL);
  stat_reg_counter(sdb, "sim_num_refs",
		   "total number of loads and stores executed",
		   &sim_num_refs, 0, NULL);
  stat_reg_int(sdb, "sim_elapsed_time",
	       "total simulation time in seconds",
	       &sim_elapsed_time, 0, NULL);
  stat_reg_formula(sdb, "sim_inst_rate",
		   "simulation speed (in insts/sec)",
		   "sim_num_insn / sim_elapsed_time", NULL);

  ld_reg_stats(sdb);
  mem_reg_stats(mem, sdb);

		   
  stat_reg_counter(sdb, "sim_num_br", "total number of conditional branches", &sim_num_br, sim_num_br, NULL);

  stat_reg_counter(sdb, "sim_num_mispred_static", "Static preditor: number of mispredicted branches", &sim_num_mispred_static, sim_num_mispred_static, NULL);
  stat_reg_formula(sdb, "sim_br_static_ratio", "Static predictor: branch misprediction rate", "sim_num_mispred_static / sim_num_br", NULL);	
  stat_reg_counter(sdb, "sim_num_mispred_random", "Random preditor: number of mispredicted branches", &sim_num_mispred_random, sim_num_mispred_random, NULL);
  stat_reg_formula(sdb, "sim_br_random_ratio", "Random predictor: branch misprediction rate", "sim_num_mispred_random / sim_num_br", NULL);	
  stat_reg_counter(sdb, "sim_num_mispred_2bitsat", "2-bit sat. counter preditor: number of mispred. branches", &sim_num_mispred_2bitsat, sim_num_mispred_2bitsat, NULL);
  stat_reg_formula(sdb, "sim_br_2bitsat_ratio", "2-bit sat. counter predictor: branch misprediction rate", "sim_num_mispred_2bitsat / sim_num_br", NULL);	
  stat_reg_counter(sdb, "sim_num_mispred_2level", "Two level preditor: number of mispredicted branches", &sim_num_mispred_2level, sim_num_mispred_2level, NULL);
  stat_reg_formula(sdb, "sim_br_2level_ratio", "Two level predictor: branch misprediction rate", "sim_num_mispred_2level / sim_num_br", NULL);	
  stat_reg_counter(sdb, "sim_num_mispred_openend", "Open-ended preditor: number of mispredicted branches", &sim_num_mispred_openend, sim_num_mispred_openend, NULL);
  stat_reg_formula(sdb, "sim_br_openend_ratio", "Open-ended predictor: branch misprediction rate", "sim_num_mispred_openend / sim_num_br", NULL);	



}

/* initialize the simulator */
void
sim_init(void)
{
  sim_num_refs = 0;

  /* allocate and initialize register file */
  regs_init(&regs);

  /* allocate and initialize memory space */
  mem = mem_create("mem");
  mem_init(mem);
}

/* load program into simulated state */
void
sim_load_prog(char *fname,		/* program to load */
	      int argc, char **argv,	/* program arguments */
	      char **envp)		/* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);

  /* initialize the DLite debugger */
  dlite_init(md_reg_obj, dlite_mem_obj, dlite_mstate_obj);
}

/* print simulator-specific configuration information */
void
sim_aux_config(FILE *stream)		/* output stream */
{
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void
sim_aux_stats(FILE *stream)		/* output stream */
{
  /* nada */
}

/* un-initialize simulator-specific state */
void
sim_uninit(void)
{
  /* nada */
}


/*
 * configure the execution engine
 */

/*
 * precise architected register accessors
 */

/* next program counter */
#define SET_NPC(EXPR)		(regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC			(regs.regs_PC)

/* general purpose registers */
#define GPR(N)			(regs.regs_R[N])
#define SET_GPR(N,EXPR)		(regs.regs_R[N] = (EXPR))

#if defined(TARGET_PISA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)		(regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)	(regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)		(regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)	(regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)		(regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR)	(regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)		(regs.regs_C.hi = (EXPR))
#define HI			(regs.regs_C.hi)
#define SET_LO(EXPR)		(regs.regs_C.lo = (EXPR))
#define LO			(regs.regs_C.lo)
#define FCC			(regs.regs_C.fcc)
#define SET_FCC(EXPR)		(regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_Q(N)		(regs.regs_F.q[N])
#define SET_FPR_Q(N,EXPR)	(regs.regs_F.q[N] = (EXPR))
#define FPR(N)			(regs.regs_F.d[(N)])
#define SET_FPR(N,EXPR)		(regs.regs_F.d[(N)] = (EXPR))

/* miscellaneous register accessors */
#define FPCR			(regs.regs_C.fpcr)
#define SET_FPCR(EXPR)		(regs.regs_C.fpcr = (EXPR))
#define UNIQ			(regs.regs_C.uniq)
#define SET_UNIQ(EXPR)		(regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST)	sys_syscall(&regs, mem_access, mem, INST, TRUE)

/* ECE552 Assignment 2 - BEGIN CODE*/ 
#define PARANOIA 0

//function for 2level
    void twobitsat(int *counter)
    {
        if(regs.regs_NPC==regs.regs_PC + 8) //if branch was not taken
        {
            if(counter[GHR]==0)//predicted not taken
                counter[GHR]=3;//correct
            else if(counter[GHR]==1)//predicted taken
            {
                counter[GHR]=2;
                sim_num_mispred_2level++;//incorrect
            }
            else if(counter[GHR]==2)//predict taken
            {
                counter[GHR]=3;
                sim_num_mispred_2level++;//incorrect
            }
#if PARANOIA
            else if(counter[GHR]==3)//predict not taken
                counter[GHR]=3;//correct
            else
                panic("WHAT IS HAPPENING I DON'T EVEN");
#endif
            GHR = (GHR << 1) & 0x1FF;
        }
        else//if branch was taken
        {            
            if(counter[GHR]==0)//predict not taken
            {
                counter[GHR]=1;
                sim_num_mispred_2level++;//incorrect
            }
#if PARANOIA
            else if(counter[GHR]==1)//predict taken
                counter[GHR]=1;//correct
#endif
            else if(counter[GHR]==2)//predict taken
                counter[GHR]=1;//correct
            else if(counter[GHR]==3)//predict not taken
            {
                counter[GHR]=0;
                sim_num_mispred_2level++;//incorrect
            }
#if PARANOIA
            else
                panic("WHAT IS HAPPENING I DON'T EVEN");
#endif
            GHR = ((GHR << 1) & 0x1FF )+1;
        }
    }
    
    void openpredict(int *counter)
    {
        if(regs.regs_NPC==regs.regs_PC + 8) //if branch was not taken
        {
            if(counter[OGHR]==0)//predicted not taken
                counter[OGHR]=3;//correct
            else if(counter[OGHR]==1)//predicted taken
            {
                counter[OGHR]=2;
                sim_num_mispred_openend++;//incorrect
            }
            else if(counter[OGHR]==2)//predict taken
            {
                counter[OGHR]=0;//3;
                sim_num_mispred_openend++;//incorrect
            }
#if PARANOIA
            else if(counter[OGHR]==3)//predict not taken
                counter[OGHR]=3;//correct
            else
                panic("WHAT IS HAPPENING I DON'T EVEN");
#endif
            OGHR = (OGHR << 1) & OLINESBITS;
        }
        else//if branch was taken
        {            
            if(counter[OGHR]==0)//predict not taken
            {
                counter[OGHR]=2;//1;
                sim_num_mispred_openend++;//incorrect
            }
#if PARANOIA
            else if(counter[OGHR]==1)//predict taken
                counter[OGHR]=1;//correct
#endif
            else if(counter[OGHR]==2)//predict taken
                counter[OGHR]=1;//correct
            else if(counter[OGHR]==3)//predict not taken
            {
                counter[OGHR]=0;
                sim_num_mispred_openend++;//incorrect
            }
#if PARANOIA
            else
                panic("WHAT IS HAPPENING I DON'T EVEN");
#endif
            OGHR = ((OGHR << 1) & OLINESBITS )+1;
        }
    }
    
/* ECE552 Assignment 2 - END CODE*/ 

/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void)
{
  md_inst_t inst;
  register md_addr_t addr;
  enum md_opcode op;
  register int is_write;
  enum md_fault_type fault;

/* ECE552 Assignment 2 - BEGIN CODE*/ 

#if PARANOIA
	int i;
	for(i=0;i<4096;i++)
		twobit[i]=0;
	int j;
	for(j=0;j<8;j++)
	    for(i=0;i<512;i++)
	        twolvl[j][i]=0;
	for(j=0;j<OTABLES;j++)
		for(i=0;i<OLINES;i++)
			open[j][i]=0;//2;//weakly taken
#endif
	        
/* ECE552 Assignment 2 - END CODE*/ 


  fprintf(stderr, "sim: ** starting functional simulation **\n");

  /* set up initial default next PC */
  regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);

  /* check for DLite debugger entry condition */
  if (dlite_check_break(regs.regs_PC, /* !access */0, /* addr */0, 0, 0))
    dlite_main(regs.regs_PC - sizeof(md_inst_t),
	       regs.regs_PC, sim_num_insn, &regs, mem);

  while (TRUE)
    {
      /* maintain $r0 semantics */
      regs.regs_R[MD_REG_ZERO] = 0;
#ifdef TARGET_ALPHA
      regs.regs_F.d[MD_REG_ZERO] = 0.0;
#endif /* TARGET_ALPHA */

      /* get the next instruction to execute */
      MD_FETCH_INST(inst, mem, regs.regs_PC);

      /* keep an instruction count */
      sim_num_insn++;
	  
      /* set default reference address and access mode */
      addr = 0; is_write = FALSE;

      /* set default fault - none */
      fault = md_fault_none;

      /* decode the instruction */
      MD_SET_OPCODE(op, inst);

      /* execute the instruction */
      switch (op)
	{
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
	case OP:							\
          SYMCAT(OP,_IMPL);						\
          break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
        case OP:							\
          panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)						\
	  { fault = (FAULT); break; }
#include "machine.def"
	default:
	  panic("attempted to execute a bogus opcode");
      }

/* ECE552 Assignment 2 - BEGIN CODE*/

    //if we get to a conditional integer or floating point
    if( MD_OP_FLAGS(op) & F_COND )
    {
        //Conditional branch encountered
        sim_num_br++;
        
        //Randomly predict if branch was taken
        int random_taken = rand()%2;//1: taken. 0: not taken
        
        int indextwobit = (regs.regs_PC >> 3) & 0xFFF;

        if(regs.regs_NPC==regs.regs_PC + 8) //if branch was not taken
        {
            //STATIC predicted taken, was not taken
            sim_num_mispred_static ++;//misprediction ++
            
            //RANDOM predicted taken, was not taken
            if(random_taken)
                sim_num_mispred_random++;//misprediction++
            
            //2-BIT SATURATED
            if(twobit[indextwobit]==0)//predicted not taken
                twobit[indextwobit]=3;//correct
            else if(twobit[indextwobit]==1)//predicted taken
            {
                twobit[indextwobit]=2;
                sim_num_mispred_2bitsat++;//incorrect
            }
            else if(twobit[indextwobit]==2)//predict taken
            {
                twobit[indextwobit]=3;
                sim_num_mispred_2bitsat++;//incorrect
            }
#if PARANOIA
            else if(twobit[indextwobit]==3)//predict not taken
                twobit[indextwobit]=3;//correct
            else
                panic("WHAT IS HAPPENING I DON'T EVEN");
#endif
        }
        else//if branch was taken
        {
            //RANDOM predicted not taken, was taken
            if(!random_taken)
                sim_num_mispred_random++;//misprediction++
            
            //2-BIT SATURATED
            if(twobit[indextwobit]==0)//predict not taken
            {
                twobit[indextwobit]=1;
                sim_num_mispred_2bitsat++;//incorrect
            }
#if PARANOIA
            else if(twobit[indextwobit]==1)//predict taken
                twobit[indextwobit]=1;//correct
#endif
            else if(twobit[indextwobit]==2)//predict taken
                twobit[indextwobit]=1;//correct
            else if(twobit[indextwobit]==3)//predict not taken
            {
                twobit[indextwobit]=0;
                sim_num_mispred_2bitsat++;//incorrect
            }
#if PARANOIA
            else
                panic("WHAT IS HAPPENING I DON'T EVEN");
#endif
        }
        //2-LEVEL
        int indextwolvl = (regs.regs_PC >> 3) & 0x7;
        twobitsat(twolvl[indextwolvl]);
        
        //OPEN
        int opentable=(regs.regs_PC >> 3)  & OTABLESBITS;
        openpredict(open[opentable]);
    }
/* ECE552 Assignment 2 - END CODE*/


      if (fault != md_fault_none)
	fatal("fault (%d) detected @ 0x%08p", fault, regs.regs_PC);

      if (verbose)
	{
	  myfprintf(stderr, "%10n [xor: 0x%08x] @ 0x%08p: ",
		    sim_num_insn, md_xor_regs(&regs), regs.regs_PC);
	  md_print_insn(inst, regs.regs_PC, stderr);
	  if (MD_OP_FLAGS(op) & F_MEM)
	    myfprintf(stderr, "  mem: 0x%08p", addr);
	  fprintf(stderr, "\n");
	  /* fflush(stderr); */
	}

      if (MD_OP_FLAGS(op) & F_MEM)
	{
	  sim_num_refs++;
	  if (MD_OP_FLAGS(op) & F_STORE)
	    is_write = TRUE;
	}

      /* check for DLite debugger entry condition */
      if (dlite_check_break(regs.regs_NPC,
			    is_write ? ACCESS_WRITE : ACCESS_READ,
			    addr, sim_num_insn, sim_num_insn))
	dlite_main(regs.regs_PC, regs.regs_NPC, sim_num_insn, &regs, mem);

      /* go to the next instruction */
      regs.regs_PC = regs.regs_NPC;
      regs.regs_NPC += sizeof(md_inst_t);

      /* finish early? */
      if (max_insts && sim_num_insn >= max_insts)
	return;
    }
}
