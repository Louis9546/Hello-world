/* bpred.c - branch predictor routines */

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
#include <assert.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "bpred.h"

/* turn this on to enable the SimpleScalar 2.0 RAS bug */
/* #define RAS_BUG_COMPATIBLE */

/* create a branch predictor */
struct bpred_t *			/* branch predictory instance */
bpred_create(enum bpred_class class,	/* type of predictor to create */
	     unsigned int bimod_size,	/* bimod table size */
	     unsigned int l1size,	/* 2lev l1 table size */
	     unsigned int l2size,	/* 2lev l2 table size */
	     unsigned int meta_size,	/* meta table size */
	     unsigned int shift_width,	/* history register width */
	     unsigned int xor,  	/* history xor address flag */
	     unsigned int btb_sets,	/* number of sets in BTB */ 
	     unsigned int btb_assoc,	/* BTB associativity */
	     unsigned int retstack_size) /* num entries in ret-addr stack */
{
  struct bpred_t *pred;

  if (!(pred = calloc(1, sizeof(struct bpred_t))))
    fatal("out of virtual memory");

  pred->class = class;

  switch (class) {
  case BPredComb:
    /* bimodal component */
    pred->dirpred.bimod = 
      bpred_dir_create(BPred2bit, bimod_size, 0, 0, 0);

    /* 2-level component */
    pred->dirpred.twolev = 
      bpred_dir_create(BPred2Level, l1size, l2size, shift_width, xor);

    /* metapredictor component */
    pred->dirpred.meta = 
      bpred_dir_create(BPred2bit, meta_size, 0, 0, 0);

    break;

  case BPredHodge:
#if HODGE_NEW_STRUCT
  /* bimod_size = history fifo size*/
  /* l1size = number of associativity */
    pred->dirpred.hodge =
      bpred_dir_create(class, bimod_size, l1size, 0, 1);
#else
    bimod_size = 1 << shift_width;
    l1size = 1;
    l2size = 1 << shift_width;
    meta_size = 1 << shift_width;
    xor = 1;
    /* bimodal component */
    pred->dirpred.bimod = 
      bpred_dir_create(BPred2bit, bimod_size, 0, 0, 0);

    /* 2-level component */
    pred->dirpred.twolev = 
      bpred_dir_create(BPred2Level, l1size, l2size, shift_width, xor);

    /* metapredictor component */
    pred->dirpred.meta = 
      bpred_dir_create(BPred2bit, meta_size, 0, 0, 0);
#endif
    break;


  case BPred2Level:
    pred->dirpred.twolev = 
      bpred_dir_create(class, l1size, l2size, shift_width, xor);

    break;

  case BPredGshare://create Interface
    l1size = 1;
    l2size = 1 << shift_width;
    xor = 1;
    pred->dirpred.twolev = 
      bpred_dir_create(class, l1size, l2size, shift_width, xor);//parameter 
    break;

  case BPred2bit:
    pred->dirpred.bimod = 
      bpred_dir_create(class, bimod_size, 0, 0, 0);
    /* New break for insure */
    break;
  case BPredTaken:
  case BPredNotTaken:
    /* no other state */
    break;
  /* Add new case: BPredHash */
  case BPredHash:
    pred->dirpred.hash = 
      bpred_dir_create(class, bimod_size, 0, 0, 0);
    break;
  default:
    panic("bogus predictor class");
  }

  /* allocate ret-addr stack */
  switch (class) {
  case BPredHodge:
  {
    int i=0;
#if HODGE_NEW_STRUCT
  /* bimod_size = history fifo size*/
  /* l1size = number of associativity */
    btb_sets = 1 << bimod_size;
    btb_assoc = l1size;
    printf("BTBSETS:%d,BTBASS:%d\n",btb_sets,btb_assoc);
#else

    btb_sets = 1 << shift_width;
    btb_assoc = 1;
#endif
      if (!(pred->btb.btb_data = calloc(btb_sets * btb_assoc,
          sizeof(struct bpred_btb_ent_t))))
  fatal("cannot allocate BTB");

      pred->btb.sets = btb_sets;
      pred->btb.assoc = btb_assoc;

      if (pred->btb.assoc > 1)
      for (i=0; i < (pred->btb.assoc*pred->btb.sets); i++)
      {
        if (i % pred->btb.assoc != pred->btb.assoc - 1)
          pred->btb.btb_data[i].next = &pred->btb.btb_data[i+1];
        else
          pred->btb.btb_data[i].next = NULL;
      
        if (i % pred->btb.assoc != pred->btb.assoc - 1)
          pred->btb.btb_data[i+1].prev = &pred->btb.btb_data[i];
      }

      /* allocate retstack */
      if ((retstack_size & (retstack_size-1)) != 0)
  fatal("Return-address-stack size must be zero or a power of two");
      
      pred->retstack.size = retstack_size;
      if (retstack_size)
        if (!(pred->retstack.stack = calloc(retstack_size, 
              sizeof(struct bpred_btb_ent_t))))
    fatal("cannot allocate return-address-stack");
      pred->retstack.tos = retstack_size - 1;
      
      break;
  }
  case BPred2Level:
  case BPredGshare:
  case BPred2bit:
    {
      int i;

      /* allocate BTB */
      if (!btb_sets || (btb_sets & (btb_sets-1)) != 0)//ensure the parameter and allocate the btb
	fatal("number of BTB sets must be non-zero and a power of two");
      if (!btb_assoc || (btb_assoc & (btb_assoc-1)) != 0)
	fatal("BTB associativity must be non-zero and a power of two");

      if (!(pred->btb.btb_data = calloc(btb_sets * btb_assoc,//if it 0,it cannot allocat. Else if it is 1, it succeed in allocating btb.
					sizeof(struct bpred_btb_ent_t))))
	fatal("cannot allocate BTB");

      pred->btb.sets = btb_sets;
      pred->btb.assoc = btb_assoc;

      if (pred->btb.assoc > 1)
	for (i=0; i < (pred->btb.assoc*pred->btb.sets); i++)
	  {
	    if (i % pred->btb.assoc != pred->btb.assoc - 1)
	      pred->btb.btb_data[i].next = &pred->btb.btb_data[i+1];
	    else
	      pred->btb.btb_data[i].next = NULL;
	    
	    if (i % pred->btb.assoc != pred->btb.assoc - 1)
	      pred->btb.btb_data[i+1].prev = &pred->btb.btb_data[i];
	  }

      /* allocate retstack */
      if ((retstack_size & (retstack_size-1)) != 0)
	fatal("Return-address-stack size must be zero or a power of two");
      
      pred->retstack.size = retstack_size;
      if (retstack_size)
	if (!(pred->retstack.stack = calloc(retstack_size, 
					    sizeof(struct bpred_btb_ent_t))))
	  fatal("cannot allocate return-address-stack");
      pred->retstack.tos = retstack_size - 1;
      
      break;
    }

  case BPredTaken:
  case BPredNotTaken:
    break;
  /* Add new case */
  case BPredHash:
    if (!bimod_size || (bimod_size & (bimod_size-1)) != 0)
      fatal("number of hash table must be non-zero and a power of two");
    pred->btb.sets = bimod_size;
    if (!(pred->btb.btb_data = calloc(bimod_size, sizeof(struct bpred_btb_ent_t))))
      fatal("cannot allocate BTB");
    break;

  default:
    panic("bogus predictor class");
  }

  return pred;
}

/* create a branch direction predictor */
struct bpred_dir_t *		/* branch direction predictor instance */
bpred_dir_create (
  enum bpred_class class,	/* type of predictor to create */
  unsigned int l1size,	 	/* level-1 table size */
  unsigned int l2size,	 	/* level-2 table size (if relevant) */
  unsigned int shift_width,	/* history register width */
  unsigned int xor)	    	/* history xor address flag */
{
  struct bpred_dir_t *pred_dir;
  unsigned int cnt;
  int flipflop;

  if (!(pred_dir = calloc(1, sizeof(struct bpred_dir_t))))
    fatal("out of virtual memory");

  pred_dir->class = class;

  cnt = -1;
  switch (class) {
#if HODGE_NEW_STRUCT
  case BPredHodge:
    {
      int table_size;
      /* check history fifo size */
      if (!l1size || l1size >30)
        fatal("shift_width, %d, must be non-zero and less than 31",
          l1size);
      pred_dir->config.hopo.hsets = 1 << l1size;
      pred_dir->config.hopo.hfifo = l1size;
      /* check associative num */
      if (!l2size || (l2size & (l2size - 1)) != 0)
        fatal("associative num, %d, must be non-zero and a power of two",
          l2size);
      pred_dir->config.hopo.hassoc = l2size;

      table_size = (1 << l1size) * l2size;
      /* allocate three tables */
      /* first bimod table */
      if (!(pred_dir->config.hopo.hop1_data = calloc(table_size,
        sizeof(unsigned char))))
        fatal("cannot allocate first table");
      if (!(pred_dir->config.hopo.hop1_addr = calloc(table_size,
        sizeof(unsigned long long))))
        fatal("cannot allocate first addr table");

      /* second bimod table */
      if (!(pred_dir->config.hopo.hop2_data = calloc(table_size,
        sizeof(unsigned char))))
        fatal("cannot allocate second table");
      if (!(pred_dir->config.hopo.hop2_addr = calloc(table_size,
        sizeof(unsigned long long))))
        fatal("cannot allocate second addr table");
      /* choice table */
      if (!(pred_dir->config.hopo.hopc_data = calloc(table_size,
        sizeof(unsigned char))))
        fatal("cannot allocate choice table");
      if (!(pred_dir->config.hopo.hopc_addr = calloc(table_size,
        sizeof(unsigned long long))))
        fatal("cannot allocate choice addr table");

      flipflop = 2;
      /* initialize counters to weakly taken */
      for (cnt = 0; cnt < table_size; cnt ++) {
        pred_dir->config.hopo.hop1_data[cnt] = flipflop;
        pred_dir->config.hopo.hop2_data[cnt] = flipflop;
      /* initialize choosing the first predictor */
        pred_dir->config.hopo.hopc_data[cnt] = 3 - flipflop;
      /* initialize addr */
        pred_dir->config.hopo.hop1_addr[cnt] = 0;
        pred_dir->config.hopo.hop2_addr[cnt] = 0;
        pred_dir->config.hopo.hopc_addr[cnt] = 0;
      }

      break;
    }
#endif
  case BPredGshare://detailed/specific information of creator of gshare
  case BPred2Level:
    {
      if (!l1size || (l1size & (l1size-1)) != 0)
	fatal("level-1 size, `%d', must be non-zero and a power of two",//restriction of parameter 
	      l1size);
      pred_dir->config.two.l1size = l1size;
      
      if (!l2size || (l2size & (l2size-1)) != 0)
	fatal("level-2 size, `%d', must be non-zero and a power of two", 
	      l2size);
      pred_dir->config.two.l2size = l2size;
      
      if (!shift_width || shift_width > 30)
	fatal("shift register width, `%d', must be non-zero and positive",
	      shift_width);
      pred_dir->config.two.shift_width = shift_width;		
      
      pred_dir->config.two.xor = xor;
      pred_dir->config.two.shiftregs = calloc(l1size, sizeof(int));//allocate the memory.
      if (!pred_dir->config.two.shiftregs)
	fatal("cannot allocate shift register table");
      
      pred_dir->config.two.l2table = calloc(l2size, sizeof(unsigned char));
      if (!pred_dir->config.two.l2table)
	fatal("cannot allocate second level table");

      /* initialize counters to weakly this-or-that */
      flipflop = 1;
      for (cnt = 0; cnt < l2size; cnt++)
	{
	  pred_dir->config.two.l2table[cnt] = flipflop;
	  flipflop = 3 - flipflop;
	}

      break;
    }

  case BPred2bit:
    if (!l1size || (l1size & (l1size-1)) != 0)
      fatal("2bit table size, `%d', must be non-zero and a power of two", 
	    l1size);
    pred_dir->config.bimod.size = l1size;
    if (!(pred_dir->config.bimod.table =
	  calloc(l1size, sizeof(unsigned char))))
      fatal("cannot allocate 2bit storage");
    /* initialize counters to weakly this-or-that */
    flipflop = 1;
    for (cnt = 0; cnt < l1size; cnt++)
      {
	pred_dir->config.bimod.table[cnt] = flipflop;
	flipflop = 3 - flipflop;
      }

    break;

  /* Add Hash case*/
  case BPredHash:
    if (!l1size || (l1size & (l1size-1)) != 0) {
      fatal("table size must be non zero and a power of 2");
    }
    pred_dir->config.ha.hasize = l1size;
    if (!(pred_dir->config.ha.hatable = calloc(l1size, sizeof(int)))) {
      fatal("cannot allocate hash size table");
    }  

    break;
  case BPredTaken:
  case BPredNotTaken:
    /* no other state */
    break;

  default:
    panic("bogus branch direction predictor class");
  }

  return pred_dir;
}

/* print branch direction predictor configuration */
void
bpred_dir_config(
  struct bpred_dir_t *pred_dir,	/* branch direction predictor instance */
  char name[],			/* predictor name */
  FILE *stream)			/* output stream */
{
  switch (pred_dir->class) {
#if HODGE_NEW_STRUCT
  case BPredHodge:
    fprintf(stream,
      "pred_dir: %s: hodge: %d sets, %d assoc, %d bits/ent\n",
      name, pred_dir->config.hopo.hsets, pred_dir->config.hopo.hassoc,
      pred_dir->config.hopo.hfifo);
    break;

#endif
  case BPred2Level:
    fprintf(stream,
      "pred_dir: %s: 2-lvl: %d l1-sz, %d bits/ent, %s xor, %d l2-sz, direct-mapped\n",
      name, pred_dir->config.two.l1size, pred_dir->config.two.shift_width,
      pred_dir->config.two.xor ? "" : "no", pred_dir->config.two.l2size);
    break;

  case BPredGshare://print it
    fprintf(stream, 
      "pred_dir: %s: gshare: %d l1-sz, %d bits/ent, %s xor, %d l2-sz, direct-mapped\n",
      name, pred_dir->config.two.l1size, pred_dir->config.two.shift_width,
      pred_dir->config.two.xor ? "" : "no", pred_dir->config.two.l2size);
    break;

  case BPred2bit:
    fprintf(stream, "pred_dir: %s: 2-bit: %d entries, direct-mapped\n",
      name, pred_dir->config.bimod.size);
    break;

  case BPredTaken:
    fprintf(stream, "pred_dir: %s: predict taken\n", name);
    break;

  case BPredNotTaken:
    fprintf(stream, "pred_dir: %s: predict not taken\n", name);
    break;
  case BPredHash:
    fprintf(stream, "pred_dir: %s: predict with %d entries\n", name, pred_dir->config.ha.hasize);
    break;
  default:
    panic("bogus branch direction predictor class");
  }
}

/* print branch predictor configuration */
void
bpred_config(struct bpred_t *pred,	/* branch predictor instance */
	     FILE *stream)		/* output stream */
{
  switch (pred->class) {
  case BPredComb:
    bpred_dir_config (pred->dirpred.bimod, "bimod", stream);
    bpred_dir_config (pred->dirpred.twolev, "2lev", stream);
    bpred_dir_config (pred->dirpred.meta, "meta", stream);
    fprintf(stream, "btb: %d sets x %d associativity", 
	    pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPredHodge:
#if HODGE_NEW_STRUCT
    bpred_dir_config(pred->dirpred.hodge, "hodge", stream);
#else
    bpred_dir_config (pred->dirpred.bimod, "bimod", stream);
    bpred_dir_config (pred->dirpred.twolev, "2lev", stream);
    bpred_dir_config (pred->dirpred.meta, "meta", stream);
#endif
    fprintf(stream, "btb: %d sets x %d associativity", 
      pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;   

  case BPred2Level:
    bpred_dir_config (pred->dirpred.twolev, "2lev", stream);
    fprintf(stream, "btb: %d sets x %d associativity", 
	    pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPredGshare:
    bpred_dir_config (pred->dirpred.twolev, "gshare", stream);
    fprintf(stream, "btb: %d sets x %d associativity", 
      pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPred2bit:
    bpred_dir_config (pred->dirpred.bimod, "bimod", stream);
    fprintf(stream, "btb: %d sets x %d associativity", 
	    pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPredTaken:
    bpred_dir_config (pred->dirpred.bimod, "taken", stream);
    break;
  case BPredNotTaken:
    bpred_dir_config (pred->dirpred.bimod, "nottaken", stream);
    break;
  /* Add new case here */
  case BPredHash:
    bpred_dir_config (pred->dirpred.hash, "hash", stream);
    break;

  default:
    panic("bogus branch predictor class");
  }
}

/* print predictor stats */
void
bpred_stats(struct bpred_t *pred,	/* branch predictor instance */
	    FILE *stream)		/* output stream */
{
  fprintf(stream, "pred: addr-prediction rate = %f\n",
	  (double)pred->addr_hits/(double)(pred->addr_hits+pred->misses));
  fprintf(stream, "pred: dir-prediction rate = %f\n",
	  (double)pred->dir_hits/(double)(pred->dir_hits+pred->misses));
}

/* register branch predictor stats */
void
bpred_reg_stats(struct bpred_t *pred,	/* branch predictor instance */
		struct stat_sdb_t *sdb)	/* stats database */
{
  char buf[512], buf1[512], *name;

  /* get a name for this predictor */
  switch (pred->class)
    {
    case BPredComb:
      name = "bpred_comb";
      break;
    case BPredHodge:
      name = "bpred_hodge";
      break;
    case BPred2Level:
      name = "bpred_2lev";
      break;
    case BPredGshare:
      name = "bpred_gshare";
      break;
    case BPred2bit:
      name = "bpred_bimod";
      break;
    case BPredTaken:
      name = "bpred_taken";
      break;
    case BPredNotTaken:
      name = "bpred_nottaken";
      break;
    /* Add new case */
    case BPredHash:
      name = "bpred_hash";
    break;
    default:
      panic("bogus branch predictor class");
    }

  sprintf(buf, "%s.lookups", name);
  stat_reg_counter(sdb, buf, "total number of bpred lookups",
		   &pred->lookups, 0, NULL);
  sprintf(buf, "%s.updates", name);
  sprintf(buf1, "%s.dir_hits + %s.misses", name, name);
  stat_reg_formula(sdb, buf, "total number of updates", buf1, "%12.0f");
  sprintf(buf, "%s.addr_hits", name);
  stat_reg_counter(sdb, buf, "total number of address-predicted hits", 
		   &pred->addr_hits, 0, NULL);
  sprintf(buf, "%s.dir_hits", name);
  stat_reg_counter(sdb, buf, 
		   "total number of direction-predicted hits "
		   "(includes addr-hits)", 
		   &pred->dir_hits, 0, NULL);
  if (pred->class == BPredComb)
    {
      sprintf(buf, "%s.used_bimod", name);
      stat_reg_counter(sdb, buf, 
		       "total number of bimodal predictions used", 
		       &pred->used_bimod, 0, NULL);
      sprintf(buf, "%s.used_2lev", name);
      stat_reg_counter(sdb, buf, 
		       "total number of 2-level predictions used", 
		       &pred->used_2lev, 0, NULL);
    }
  sprintf(buf, "%s.misses", name);
  stat_reg_counter(sdb, buf, "total number of misses", &pred->misses, 0, NULL);
  sprintf(buf, "%s.jr_hits", name);
  stat_reg_counter(sdb, buf,
		   "total number of address-predicted hits for JR's",
		   &pred->jr_hits, 0, NULL);
  sprintf(buf, "%s.jr_seen", name);
  stat_reg_counter(sdb, buf,
		   "total number of JR's seen",
		   &pred->jr_seen, 0, NULL);
  sprintf(buf, "%s.jr_non_ras_hits.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of address-predicted hits for non-RAS JR's",
		   &pred->jr_non_ras_hits, 0, NULL);
  sprintf(buf, "%s.jr_non_ras_seen.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of non-RAS JR's seen",
		   &pred->jr_non_ras_seen, 0, NULL);
  sprintf(buf, "%s.bpred_addr_rate", name);
  sprintf(buf1, "%s.addr_hits / %s.updates", name, name);
  stat_reg_formula(sdb, buf,
		   "branch address-prediction rate (i.e., addr-hits/updates)",
		   buf1, "%9.4f");
  sprintf(buf, "%s.bpred_dir_rate", name);
  sprintf(buf1, "%s.dir_hits / %s.updates", name, name);
  stat_reg_formula(sdb, buf,
		  "branch direction-prediction rate (i.e., all-hits/updates)",
		  buf1, "%9.4f");
  sprintf(buf, "%s.bpred_jr_rate", name);
  sprintf(buf1, "%s.jr_hits / %s.jr_seen", name, name);
  stat_reg_formula(sdb, buf,
		  "JR address-prediction rate (i.e., JR addr-hits/JRs seen)",
		  buf1, "%9.4f");
  sprintf(buf, "%s.bpred_jr_non_ras_rate.PP", name);
  sprintf(buf1, "%s.jr_non_ras_hits.PP / %s.jr_non_ras_seen.PP", name, name);
  stat_reg_formula(sdb, buf,
		   "non-RAS JR addr-pred rate (ie, non-RAS JR hits/JRs seen)",
		   buf1, "%9.4f");
  sprintf(buf, "%s.retstack_pushes", name);
  stat_reg_counter(sdb, buf,
		   "total number of address pushed onto ret-addr stack",
		   &pred->retstack_pushes, 0, NULL);
  sprintf(buf, "%s.retstack_pops", name);
  stat_reg_counter(sdb, buf,
		   "total number of address popped off of ret-addr stack",
		   &pred->retstack_pops, 0, NULL);
  sprintf(buf, "%s.used_ras.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of RAS predictions used",
		   &pred->used_ras, 0, NULL);
  sprintf(buf, "%s.ras_hits.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of RAS hits",
		   &pred->ras_hits, 0, NULL);
  sprintf(buf, "%s.ras_rate.PP", name);
  sprintf(buf1, "%s.ras_hits.PP / %s.used_ras.PP", name, name);
  stat_reg_formula(sdb, buf,
		   "RAS prediction rate (i.e., RAS hits/used RAS)",
		   buf1, "%9.4f");
}

void
bpred_after_priming(struct bpred_t *bpred)
{
  if (bpred == NULL)
    return;

  bpred->lookups = 0;
  bpred->addr_hits = 0;
  bpred->dir_hits = 0;
  bpred->used_ras = 0;
  bpred->used_bimod = 0;
  bpred->used_2lev = 0;
  bpred->jr_hits = 0;
  bpred->jr_seen = 0;
  bpred->misses = 0;
  bpred->retstack_pops = 0;
  bpred->retstack_pushes = 0;
  bpred->ras_hits = 0;
}

#define BIMOD_HASH(PRED, ADDR)						\
  ((((ADDR) >> 19) ^ ((ADDR) >> MD_BR_SHIFT)) & ((PRED)->config.bimod.size-1))
    /* was: ((baddr >> 16) ^ baddr) & (pred->dirpred.bimod.size-1) */

/* predicts a branch direction */
#if HODGE_NEW_STRUCT
char *            /* pointer to counter */
bpred_dir_lookup(struct bpred_dir_t *pred_dir,  /* branch dir predictor inst */
     md_addr_t baddr, /* branch address */
     int point)   
{
  unsigned char *p = NULL;

  /* Except for jumps, get a pointer to direction-prediction bits */
  switch (pred_dir->class) {
    case BPredHodge:
      {
        int i;
        if (point == 1) {
          /* look for first predictor */
          int set_num1,base_ad1;
          set_num1 = (baddr >> MD_BR_SHIFT) & (pred_dir->config.hopo.hsets - 1);
          /* find p1*/
          base_ad1 = set_num1 * pred_dir->config.hopo.hassoc;
          for (i = base_ad1; i < base_ad1 + pred_dir->config.hopo.hassoc; i++) {
            if (pred_dir->config.hopo.hop1_addr[i] == baddr) {
              /* p1 match */
              p = &pred_dir->config.hopo.hop1_data[i];
            } 
          }
          if (!p) {
            /* if not found, find a place in set(LRU) */
            for(i = base_ad1; i < base_ad1 + pred_dir->config.hopo.hassoc - 1; i++){
              pred_dir->config.hopo.hop1_data[i] = pred_dir->config.hopo.hop1_data[i+1];
              pred_dir->config.hopo.hop1_addr[i] = pred_dir->config.hopo.hop1_addr[i+1];
            }
            pred_dir->config.hopo.hop1_addr[i] = baddr;
            p = &pred_dir->config.hopo.hop1_data[i];
          } 
        } else if (point == 2) {
          /* look for the second predictor */
          int base_ad2;
          int set_num2 = pred_dir->config.hopo.fifo_reg;
          set_num2 = (set_num2 ^ (baddr >> MD_BR_SHIFT))
            & ((1 << pred_dir->config.hopo.hfifo) - 1);
          base_ad2 = set_num2 * pred_dir->config.hopo.hassoc;
          for (i = base_ad2; i < base_ad2 + pred_dir->config.hopo.hassoc; i++) {
            if (pred_dir->config.hopo.hop2_addr[i] == baddr) {
              /* p1 match */
              p = &pred_dir->config.hopo.hop2_data[i];
            } 
          }
          if (!p) {
            /* if not found, find a place in set(LRU) */
            for(i = base_ad2; i < base_ad2 + pred_dir->config.hopo.hassoc - 1; i++) {
              pred_dir->config.hopo.hop2_data[i] = pred_dir->config.hopo.hop2_data[i+1];
              pred_dir->config.hopo.hop2_addr[i] = pred_dir->config.hopo.hop2_addr[i+1];
            }
            pred_dir->config.hopo.hop2_addr[i] = baddr;
            p = &pred_dir->config.hopo.hop2_data[i];
          } 
        } else if (point == 3) {
          /* look for choice */
          int set_num3;
          int base_ad3;
          set_num3 = (baddr >> MD_BR_SHIFT) & (pred_dir->config.hopo.hsets - 1);
          base_ad3 = set_num3 * pred_dir->config.hopo.hassoc;
          for (i = base_ad3; i < base_ad3 + pred_dir->config.hopo.hassoc; i++) {
            if (pred_dir->config.hopo.hopc_addr[i] == baddr) {
              /* p3 match */
              p = &pred_dir->config.hopo.hopc_data[i];
            } 
          }
          if (!p) {
            /* if not found, find a place in set(LRU) */
            for(i = base_ad3; i < base_ad3 + pred_dir->config.hopo.hassoc - 1; i++) {
              pred_dir->config.hopo.hopc_data[i] = pred_dir->config.hopo.hopc_data[i+1];
              pred_dir->config.hopo.hopc_addr[i] = pred_dir->config.hopo.hopc_addr[i+1];
            }
            pred_dir->config.hopo.hopc_addr[i] = baddr;
            p = &pred_dir->config.hopo.hopc_data[i];
          } 
        } else{
          fatal("Wrong pointer for hodge predictor\n");
        }
        break;
      }
    case BPredGshare:
    case BPred2Level:
      {
  int l1index, l2index;

        /* traverse 2-level tables */
        l1index = (baddr >> MD_BR_SHIFT) & (pred_dir->config.two.l1size - 1);
        l2index = pred_dir->config.two.shiftregs[l1index];
        if (pred_dir->config.two.xor)
    {
      /* this L2 index computation is more "compatible" to McFarling's
         verison of it, i.e., if the PC xor address component is only
         part of the index, take the lower order address bits for the
         other part of the index, rather than the higher order ones */
      l2index = (((l2index ^ (baddr >> MD_BR_SHIFT))
      & ((1 << pred_dir->config.two.shift_width) - 1))
           | ((baddr >> MD_BR_SHIFT)
        << pred_dir->config.two.shift_width));
    }
  else
    {
      l2index =
        l2index
    | ((baddr >> MD_BR_SHIFT) << pred_dir->config.two.shift_width);
    }
        l2index = l2index & (pred_dir->config.two.l2size - 1);

        /* get a pointer to prediction state information */
        p = &pred_dir->config.two.l2table[l2index];
      }
      break;
    case BPred2bit:
      p = &pred_dir->config.bimod.table[BIMOD_HASH(pred_dir, baddr)];
      break;
    /* Add new case */
    case BPredHash:
    {
      int hash_addr;
      hash_addr = ((baddr >> MD_BR_SHIFT) ^ (pred_dir->config.ha.hasize - 1)) & (pred_dir->config.ha.hasize - 1);
      p = &pred_dir->config.ha.hatable[hash_addr];
    }
      break;
    case BPredTaken:
    case BPredNotTaken:
      break;
    default:
      panic("bogus branch direction predictor class");
    }

  return (char *)p;
}

#else
char *						/* pointer to counter */
bpred_dir_lookup(struct bpred_dir_t *pred_dir,	/* branch dir predictor inst */
		 md_addr_t baddr)		/* branch address */
{
  unsigned char *p = NULL;

  /* Except for jumps, get a pointer to direction-prediction bits */
  switch (pred_dir->class) {

    case BPredGshare:
    case BPred2Level:
      {
	int l1index, l2index;

        /* traverse 2-level tables */
        l1index = (baddr >> MD_BR_SHIFT) & (pred_dir->config.two.l1size - 1);
        l2index = pred_dir->config.two.shiftregs[l1index];
        if (pred_dir->config.two.xor)
	  {
	    /* this L2 index computation is more "compatible" to McFarling's
	       verison of it, i.e., if the PC xor address component is only
	       part of the index, take the lower order address bits for the
	       other part of the index, rather than the higher order ones */
	    l2index = (((l2index ^ (baddr >> MD_BR_SHIFT))
			& ((1 << pred_dir->config.two.shift_width) - 1))
		       | ((baddr >> MD_BR_SHIFT)
			  << pred_dir->config.two.shift_width));
	  }
	else
	  {
	    l2index =
	      l2index
		| ((baddr >> MD_BR_SHIFT) << pred_dir->config.two.shift_width);
	  }
        l2index = l2index & (pred_dir->config.two.l2size - 1);

        /* get a pointer to prediction state information */
        p = &pred_dir->config.two.l2table[l2index];
      }
      break;
    case BPred2bit:
      p = &pred_dir->config.bimod.table[BIMOD_HASH(pred_dir, baddr)];
      break;
    /* Add new case */
    case BPredHash:
    {
      int hash_addr;
      hash_addr = ((baddr >> MD_BR_SHIFT) ^ (pred_dir->config.ha.hasize - 1)) & (pred_dir->config.ha.hasize - 1);
      p = &pred_dir->config.ha.hatable[hash_addr];
    }
      break;
    case BPredTaken:
    case BPredNotTaken:
      break;
    default:
      panic("bogus branch direction predictor class");
    }

  return (char *)p;
}
#endif


/* probe a predictor for a next fetch address, the predictor is probed
   with branch address BADDR, the branch target is BTARGET (used for
   static predictors), and OP is the instruction opcode (used to simulate
   predecode bits; a pointer to the predictor state entry (or null for jumps)
   is returned in *DIR_UPDATE_PTR (used for updating predictor state),
   and the non-speculative top-of-stack is returned in stack_recover_idx 
   (used for recovering ret-addr stack after mis-predict).  */
md_addr_t				/* predicted branch target addr */
bpred_lookup(struct bpred_t *pred,	/* branch predictor instance */
	     md_addr_t baddr,		/* branch address */
	     md_addr_t btarget,		/* branch target if taken */
	     enum md_opcode op,		/* opcode of instruction */
	     int is_call,		/* non-zero if inst is fn call */
	     int is_return,		/* non-zero if inst is fn return */
	     struct bpred_update_t *dir_update_ptr, /* pred state pointer */
	     int *stack_recover_idx)	/* Non-speculative top-of-stack;
					 * used on mispredict recovery */
{
  struct bpred_btb_ent_t *pbtb = NULL;
  int index, i;

  if (!dir_update_ptr)
    panic("no bpred update record");

  /* if this is not a branch, return not-taken */
  if (!(MD_OP_FLAGS(op) & F_CTRL))
    return 0;

  pred->lookups++;

  dir_update_ptr->dir.ras = FALSE;
  dir_update_ptr->pdir1 = NULL;
  dir_update_ptr->pdir2 = NULL;
  dir_update_ptr->pmeta = NULL;
  /* Except for jumps, get a pointer to direction-prediction bits */
  switch (pred->class) {
    case BPredHodge:
#if HODGE_NEW_STRUCT
  {
    if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
    {
      char *bimod, *twolev, *meta;
      bimod = bpred_dir_lookup (pred->dirpred.hodge, baddr,1);
      twolev = bpred_dir_lookup (pred->dirpred.hodge, baddr,2);
      meta = bpred_dir_lookup (pred->dirpred.hodge, baddr,3);
      dir_update_ptr->pmeta = meta;
      dir_update_ptr->dir.meta  = (*meta >= 2);
      dir_update_ptr->dir.bimod = (*bimod >= 2);
      dir_update_ptr->dir.twolev  = (*twolev >= 2);
      if (*meta >= 2)
        {
          dir_update_ptr->pdir1 = twolev;
          dir_update_ptr->pdir2 = bimod;
        }
      else
        {
          dir_update_ptr->pdir1 = bimod;
          dir_update_ptr->pdir2 = twolev;
        }
    }
    break;
    
  }
#endif
    case BPredComb:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
	  char *bimod, *twolev, *meta;
#if HODGE_NEW_STRUCT
    bimod = bpred_dir_lookup (pred->dirpred.bimod, baddr,1);
    twolev = bpred_dir_lookup (pred->dirpred.twolev, baddr,1);
    meta = bpred_dir_lookup (pred->dirpred.meta, baddr,1);    
#else 
	  bimod = bpred_dir_lookup (pred->dirpred.bimod, baddr);
	  twolev = bpred_dir_lookup (pred->dirpred.twolev, baddr);
	  meta = bpred_dir_lookup (pred->dirpred.meta, baddr);
#endif
    dir_update_ptr->pmeta = meta;
	  dir_update_ptr->dir.meta  = (*meta >= 2);
	  dir_update_ptr->dir.bimod = (*bimod >= 2);
	  dir_update_ptr->dir.twolev  = (*twolev >= 2);
	  if (*meta >= 2)
	    {
	      dir_update_ptr->pdir1 = twolev;
	      dir_update_ptr->pdir2 = bimod;
	    }
	  else
	    {
	      dir_update_ptr->pdir1 = bimod;
	      dir_update_ptr->pdir2 = twolev;
	    }
	}
      break;
    case BPredGshare:
    case BPred2Level:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
#if HODGE_NEW_STRUCT
    dir_update_ptr->pdir1 =
      bpred_dir_lookup (pred->dirpred.twolev, baddr,1);
#else
	  dir_update_ptr->pdir1 =
	    bpred_dir_lookup (pred->dirpred.twolev, baddr);
#endif
    }
      break;
    case BPred2bit:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
#if HODGE_NEW_STRUCT
    dir_update_ptr->pdir1 =
      bpred_dir_lookup (pred->dirpred.bimod, baddr,1);
#else
	  dir_update_ptr->pdir1 =
	    bpred_dir_lookup (pred->dirpred.bimod, baddr);
#endif
	}
      break;
    /* Add new case*/
    case BPredHash:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND)) {
#if HODGE_NEW_STRUCT
    dir_update_ptr->pdir1 =
      bpred_dir_lookup (pred->dirpred.hash, baddr,1);
#else
        dir_update_ptr->pdir1 = bpred_dir_lookup(pred->dirpred.hash, baddr);
#endif
      }
      break;
    case BPredTaken:
      return btarget;
    case BPredNotTaken:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
	  return baddr + sizeof(md_inst_t);
	}
      else
	{
	  return btarget;
	}
    default:
      panic("bogus predictor class");
  }

  /*
   * We have a stateful predictor, and have gotten a pointer into the
   * direction predictor (except for jumps, for which the ptr is null)
   */

  /* record pre-pop TOS; if this branch is executed speculatively
   * and is squashed, we'll restore the TOS and hope the data
   * wasn't corrupted in the meantime. */
  if (pred->retstack.size)
    *stack_recover_idx = pred->retstack.tos;
  else
    *stack_recover_idx = 0;

  /* if this is a return, pop return-address stack */
  if (is_return && pred->retstack.size)
    {
      md_addr_t target = pred->retstack.stack[pred->retstack.tos].target;
      pred->retstack.tos = (pred->retstack.tos + pred->retstack.size - 1)
	                   % pred->retstack.size;
      pred->retstack_pops++;
      dir_update_ptr->dir.ras = TRUE; /* using RAS here */
      return target;
    }

#ifndef RAS_BUG_COMPATIBLE
  /* if function call, push return-address onto return-address stack */
  if (is_call && pred->retstack.size)
    {
      pred->retstack.tos = (pred->retstack.tos + 1)% pred->retstack.size;
      pred->retstack.stack[pred->retstack.tos].target = 
	baddr + sizeof(md_inst_t);
      pred->retstack_pushes++;
    }
#endif /* !RAS_BUG_COMPATIBLE */
  
  /* not a return. Get a pointer into the BTB */
/* New condition for hash class*/
  if (pred->class == BPredHash){
    index = (baddr ^ (pred->btb.sets - 1)) &(pred->btb.sets - 1);
    /* look for a PC match*/
    if (pred->btb.btb_data[index].addr == baddr) {
      /* match */
      pbtb = &pred->btb.btb_data[index];
    } else {
      pbtb = NULL;
    }

  } else {
  /************* origin code *****************/
  index = (baddr >> MD_BR_SHIFT) & (pred->btb.sets - 1);

  if (pred->btb.assoc > 1)
    {
      index *= pred->btb.assoc;

      /* Now we know the set; look for a PC match */
      for (i = index; i < (index+pred->btb.assoc) ; i++)
	if (pred->btb.btb_data[i].addr == baddr)
	  {
	    /* match */
	    pbtb = &pred->btb.btb_data[i];
	    break;
	  }
    }	
  else
    {
      pbtb = &pred->btb.btb_data[index];
      if (pbtb->addr != baddr)
	pbtb = NULL;
    }
  }


  /*
   * We now also have a pointer into the BTB for a hit, or NULL otherwise
   */

  /* if this is a jump, ignore predicted direction; we know it's taken. */
  if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) == (F_CTRL|F_UNCOND))
    {
      return (pbtb ? pbtb->target : 1);
    }

  /* otherwise we have a conditional branch */
  if (pbtb == NULL)
    {
      /* BTB miss -- just return a predicted direction */
      return ((*(dir_update_ptr->pdir1) >= 2)
	      ? /* taken */ 1
	      : /* not taken */ 0);
    }
  else
    {
      /* BTB hit, so return target if it's a predicted-taken branch */
      return ((*(dir_update_ptr->pdir1) >= 2)
	      ? /* taken */ pbtb->target
	      : /* not taken */ 0);
    }
}

/* Speculative execution can corrupt the ret-addr stack.  So for each
 * lookup we return the top-of-stack (TOS) at that point; a mispredicted
 * branch, as part of its recovery, restores the TOS using this value --
 * hopefully this uncorrupts the stack. */
void
bpred_recover(struct bpred_t *pred,	/* branch predictor instance */
	      md_addr_t baddr,		/* branch address */
	      int stack_recover_idx)	/* Non-speculative top-of-stack;
					 * used on mispredict recovery */
{
  if (pred == NULL)
    return;

  pred->retstack.tos = stack_recover_idx;
}

/* update the branch predictor, only useful for stateful predictors; updates
   entry for instruction type OP at address BADDR.  BTB only gets updated
   for branches which are taken.  Inst was determined to jump to
   address BTARGET and was taken if TAKEN is non-zero.  Predictor 
   statistics are updated with result of prediction, indicated by CORRECT and 
   PRED_TAKEN, predictor state to be updated is indicated by *DIR_UPDATE_PTR 
   (may be NULL for jumps, which shouldn't modify state bits).  Note if
   bpred_update is done speculatively, branch-prediction may get polluted. */
void
bpred_update(struct bpred_t *pred,	/* branch predictor instance */
	     md_addr_t baddr,		/* branch address */
	     md_addr_t btarget,		/* resolved branch target */
	     int taken,			/* non-zero if branch was taken */
	     int pred_taken,		/* non-zero if branch was pred taken */
	     int correct,		/* was earlier addr prediction ok? */
	     enum md_opcode op,		/* opcode of instruction */
	     struct bpred_update_t *dir_update_ptr)/* pred state pointer */
{
  struct bpred_btb_ent_t *pbtb = NULL;
  struct bpred_btb_ent_t *lruhead = NULL, *lruitem = NULL;
  int index, i;
  /* don't change bpred state for non-branch instructions or if this
   * is a stateless predictor*/
  if (!(MD_OP_FLAGS(op) & F_CTRL))
    return;

  /* Have a branch here */

  if (correct)
    pred->addr_hits++;

  if (!!pred_taken == !!taken)
    pred->dir_hits++;
  else
    pred->misses++;

  if (dir_update_ptr->dir.ras)
    {
      pred->used_ras++;
      if (correct)
	pred->ras_hits++;
    }
  else if ((MD_OP_FLAGS(op) & (F_CTRL|F_COND)) == (F_CTRL|F_COND))
    {
      if (dir_update_ptr->dir.meta)
	pred->used_2lev++;
      else
	pred->used_bimod++;
    }

  /* keep stats about JR's; also, but don't change any bpred state for JR's
   * which are returns unless there's no retstack */
  if (MD_IS_INDIR(op))
    {
      pred->jr_seen++;
      if (correct)
	pred->jr_hits++;
      
      if (!dir_update_ptr->dir.ras)
	{
	  pred->jr_non_ras_seen++;
	  if (correct)
	    pred->jr_non_ras_hits++;
	}
      else
	{
	  /* return that used the ret-addr stack; no further work to do */
	  return;
	}
    }

  /* Can exit now if this is a stateless predictor */
  if (pred->class == BPredNotTaken || pred->class == BPredTaken)
    return;

  /* 
   * Now we know the branch didn't use the ret-addr stack, and that this
   * is a stateful predictor 
   */
#ifdef RAS_BUG_COMPATIBLE
  /* if function call, push return-address onto return-address stack */
  if (MD_IS_CALL(op) && pred->retstack.size)
    {
      pred->retstack.tos = (pred->retstack.tos + 1)% pred->retstack.size;
      pred->retstack.stack[pred->retstack.tos].target = 
	baddr + sizeof(md_inst_t);
      pred->retstack_pushes++;
    }
#endif /* RAS_BUG_COMPATIBLE */
  /* update L1 table if appropriate */
  /* L1 table is updated unconditionally for combining predictor too */
  if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND) &&
      (pred->class == BPred2Level || pred->class == BPredComb || pred->class == BPredGshare || pred->class == BPredHodge))
    {
      int l1index, shift_reg;
#if HODGE_NEW_STRUCT
      if (pred->class == BPredHodge) {
        shift_reg = 
          (pred->dirpred.hodge->config.hopo.fifo_reg << 1) | (!!taken);
        pred->dirpred.hodge->config.hopo.fifo_reg = 
          shift_reg & ((1 << pred->dirpred.hodge->config.hopo.hfifo) -1);
      } else {
      l1index =
  (baddr >> MD_BR_SHIFT) & (pred->dirpred.twolev->config.two.l1size - 1);
      shift_reg =
  (pred->dirpred.twolev->config.two.shiftregs[l1index] << 1) | (!!taken);
      pred->dirpred.twolev->config.two.shiftregs[l1index] =
  shift_reg & ((1 << pred->dirpred.twolev->config.two.shift_width) - 1);  
      }
#else
      /* also update appropriate L1 history register */
      l1index =
	(baddr >> MD_BR_SHIFT) & (pred->dirpred.twolev->config.two.l1size - 1);
      shift_reg =
	(pred->dirpred.twolev->config.two.shiftregs[l1index] << 1) | (!!taken);
      pred->dirpred.twolev->config.two.shiftregs[l1index] =
	shift_reg & ((1 << pred->dirpred.twolev->config.two.shift_width) - 1);
#endif
    }
  /* Add new case */
  /*if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND) &&
      (pred->class == BPredHash)) {
    l1index = baddr ^ (pred->btb.sets - 1);
  }*/
  /* find BTB entry if it's a taken branch (don't allocate for non-taken) */
  if (taken)
    {
      /* for hash case*/
      if (pred->class == BPredHash){
        index = (baddr ^ (pred->btb.sets - 1)) & (pred->btb.sets - 1);
        pbtb = &pred->btb.btb_data[index];
      } else {

      /* origin code */
      index = (baddr >> MD_BR_SHIFT) & (pred->btb.sets - 1);
      
      if (pred->btb.assoc > 1)
	{
	  index *= pred->btb.assoc;
	  
	  /* Now we know the set; look for a PC match; also identify
	   * MRU and LRU items */
	  for (i = index; i < (index+pred->btb.assoc) ; i++)
	    {
	      if (pred->btb.btb_data[i].addr == baddr)
		{
		  /* match */
		  assert(!pbtb);
		  pbtb = &pred->btb.btb_data[i];
		}
	      
	      dassert(pred->btb.btb_data[i].prev 
		      != pred->btb.btb_data[i].next);
	      if (pred->btb.btb_data[i].prev == NULL)
		{
		  /* this is the head of the lru list, ie current MRU item */
		  dassert(lruhead == NULL);
		  lruhead = &pred->btb.btb_data[i];
		}
	      if (pred->btb.btb_data[i].next == NULL)
		{
		  /* this is the tail of the lru list, ie the LRU item */
		  dassert(lruitem == NULL);
		  lruitem = &pred->btb.btb_data[i];
		}
	    }
	  dassert(lruhead && lruitem);
	  
	  if (!pbtb)
	    /* missed in BTB; choose the LRU item in this set as the victim */
	    pbtb = lruitem;	
	  /* else hit, and pbtb points to matching BTB entry */
	  
	  /* Update LRU state: selected item, whether selected because it
	   * matched or because it was LRU and selected as a victim, becomes 
	   * MRU */
	  if (pbtb != lruhead)
	    {
	      /* this splices out the matched entry... */
	      if (pbtb->prev)
		pbtb->prev->next = pbtb->next;
	      if (pbtb->next)
		pbtb->next->prev = pbtb->prev;
	      /* ...and this puts the matched entry at the head of the list */
	      pbtb->next = lruhead;
	      pbtb->prev = NULL;
	      lruhead->prev = pbtb;
	      dassert(pbtb->prev || pbtb->next);
	      dassert(pbtb->prev != pbtb->next);
	    }
	  /* else pbtb is already MRU item; do nothing */
	}
      else
	pbtb = &pred->btb.btb_data[index];
  }
    }
      
  /* 
   * Now 'p' is a possibly null pointer into the direction prediction table, 
   * and 'pbtb' is a possibly null pointer into the BTB (either to a 
   * matched-on entry or a victim which was LRU in its set)
   */

  /* update state (but not for jumps) */
  if (dir_update_ptr->pdir1)
    {
      if (taken)
	{
	  if (*dir_update_ptr->pdir1 < 3)
	    ++*dir_update_ptr->pdir1;
	}
      else
	{ /* not taken */
	  if (*dir_update_ptr->pdir1 > 0)
	    --*dir_update_ptr->pdir1;
	}
    }

  /* combining predictor also updates second predictor and meta predictor */
  /* second direction predictor */
  if (dir_update_ptr->pdir2)
    {
      if (taken)
	{
	  if (*dir_update_ptr->pdir2 < 3)
	    ++*dir_update_ptr->pdir2;
	}
      else
	{ /* not taken */
	  if (*dir_update_ptr->pdir2 > 0)
	    --*dir_update_ptr->pdir2;
	}
    }

  /* meta predictor */
  if (dir_update_ptr->pmeta)
    {
      if (dir_update_ptr->dir.bimod != dir_update_ptr->dir.twolev)
	{
	  /* we only update meta predictor if directions were different */
	  if (dir_update_ptr->dir.twolev == (unsigned int)taken)
	    {
	      /* 2-level predictor was correct */
	      if (*dir_update_ptr->pmeta < 3)
		++*dir_update_ptr->pmeta;
	    }
	  else
	    {
	      /* bimodal predictor was correct */
	      if (*dir_update_ptr->pmeta > 0)
		--*dir_update_ptr->pmeta;
	    }
	}
    }
  /* update BTB (but only for taken branches) */
  if (pbtb)
    {
      /* update current information */
      dassert(taken);

      if (pbtb->addr == baddr)
	{
	  if (!correct)
	    pbtb->target = btarget;
	}
      else
	{
	  /* enter a new branch in the table */
	  pbtb->addr = baddr;
	  pbtb->op = op;
	  pbtb->target = btarget;
	}
    }
}
