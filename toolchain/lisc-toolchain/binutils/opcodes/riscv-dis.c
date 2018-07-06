/* RISC-V disassembler
   Copyright 2011-2015 Free Software Foundation, Inc.

   Contributed by Andrew Waterman (waterman@cs.berkeley.edu) at UC Berkeley.
   Based on MIPS target.

   PULP family support contributed by Eric Flamand (eflamand@iis.ee.ethz.ch) at ETH-Zurich

   This file is part of the GNU opcodes library.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */

#include "sysdep.h"
#include "dis-asm.h"
#include "libiberty.h"
#include "opcode/riscv.h"
#include "opintl.h"
#include "elf-bfd.h"
#include "elf/riscv.h"

#include <stdint.h>
#include <ctype.h>
#include <stdio.h>


struct riscv_private_data
{
  bfd_vma gp;
  bfd_vma print_addr;
  bfd_vma hi_addr[OP_MASK_RD + 1];
};

static const char * const *riscv_gpr_names;
static const char * const *riscv_fpr_names;

/* Other options */
static int no_aliases;	/* If set disassemble as most general inst.  */

struct riscv_subset
{
  const char *name;
  int version_major;
  int version_minor;

  struct riscv_subset *next;
};

static struct riscv_subset *riscv_subsets;

static void
riscv_add_subset (const char *subset)
{
  struct riscv_subset *s = xmalloc (sizeof *s);
  s->name = xstrdup (subset);
  s->version_major = 1;
  s->version_minor = 0;
  s->next = riscv_subsets;
  riscv_subsets = s;
}

static void
riscv_set_arch (const char *arg)
{
  char *uppercase = xstrdup (arg);
  char *p = uppercase;
  const char *all_subsets = "IMAFDC";
  const char *extension = NULL;
  int rvc = 0;
  int i;

  for (i = 0; uppercase[i]; i++) uppercase[i] = toupper (uppercase[i]);

  if (strncmp (p, "RV32", 4) == 0)
    {
      p += 4;
    }
  else if (strncmp (p, "RV64", 4) == 0)
    {
      p += 4;
    }
  else if (strncmp (p, "RV", 2) == 0)
    p += 2;

  switch (*p)
    {
      case 'I':
        break;

      case 'G':
        p++;
        /* Fall through.  */

      case '\0':
        for (i = 0; all_subsets[i] != '\0'; i++)
          {
            const char subset[] = {all_subsets[i], '\0'};
            riscv_add_subset (subset);
          }
        break;

      default:
        fprintf (stderr, "`I' must be the first ISA subset name specified (got %c). Ignoring -march", *p);
	riscv_subsets=NULL;
	return;
    }

  while (*p)
    {
      if (*p == 'X')
        {
          char *subset = xstrdup (p), *q = subset;

          while (*++q != '\0' && *q != '_')
            ;
          *q = '\0';

          if (extension) {
		fprintf (stderr, "only one eXtension is supported (found %s and %s). Ignoring -march", extension, subset);
		riscv_subsets=NULL;
		return;
	  }
          extension = subset;
          riscv_add_subset (subset);
          p += strlen (subset);
          free (subset);
        }
      else if (*p == '_')
        p++;
      else if ((all_subsets = strchr (all_subsets, *p)) != NULL)
        {
          const char subset[] = {*p, 0};
          riscv_add_subset (subset);
          if (*p == 'C')
            rvc = 1;
          all_subsets++;
          p++;
        }
      else {
        fprintf(stderr, "unsupported ISA subset %c. Ignoring -march", *p);
	riscv_subsets=NULL;
	return;
      }
    }

  if (!rvc)
    /* Add RVC anyway.  -m[no-]rvc toggles its availability.  */
    riscv_add_subset ("C");

  free (uppercase);
}

static void
set_default_riscv_dis_options (void)
{
  riscv_gpr_names = riscv_gpr_names_abi;
  riscv_fpr_names = riscv_fpr_names_abi;
  no_aliases = 0;
}

static void
parse_riscv_dis_option (char *option)
{
  const char *where=NULL;
  if (CONST_STRNEQ (option, "no-aliases"))
    no_aliases = 1;
  else if (CONST_STRNEQ (option, "numeric"))
    {
      riscv_gpr_names = riscv_gpr_names_numeric;
      riscv_fpr_names = riscv_fpr_names_numeric;
    }
  else if ((where = strstr(option, "-march="))&&(where==option)) {
	char *comma;
	where = option+7;
	comma = strstr(where, ",");
	if (comma) *comma = '\0';
	riscv_set_arch(where);
	if (comma) *comma = ',';
  } else if ((where = strstr(option, "march="))&&(where==option)) {
	char *comma;
	where = option+6;
	comma = strstr(where, ",");
	if (comma) *comma = '\0';
	riscv_set_arch(where);
	if (comma) *comma = ',';
  } else
    {
      /* Invalid option.  */
      fprintf (stderr, _("Unrecognized disassembler option: %s\n"), option);
    }
}

static void
parse_riscv_dis_options (const char *opts_in)
{
  char *opts = xstrdup (opts_in), *opt = opts, *opt_end = opts;

  set_default_riscv_dis_options ();

  for ( ; opt_end != NULL; opt = opt_end + 1)
    {
      if ((opt_end = strchr (opt, ',')) != NULL)
	*opt_end = 0;
      parse_riscv_dis_option (opt);
    }

  free (opts);
}

/* Print one argument from an array.  */

static void
arg_print (struct disassemble_info *info, unsigned long val,
	   const char* const* array, size_t size)
{
  const char *s = val >= size || array[val] == NULL ? "unknown" : array[val];
  (*info->fprintf_func) (info->stream, "%s", s);
}

static void
maybe_print_address (struct riscv_private_data *pd, int base_reg, int offset)
{
  if (base_reg == 0) { /* %tiny(Symbol_Expr)(x0) */
    pd->print_addr = offset;
  } else if (pd->hi_addr[base_reg] != (bfd_vma)-1)
    {
      pd->print_addr = pd->hi_addr[base_reg] + offset;
      pd->hi_addr[base_reg] = -1;
    }
  else if (base_reg == X_GP && pd->gp != (bfd_vma)-1)
    pd->print_addr = pd->gp + offset;
  else if (base_reg == X_TP)
    pd->print_addr = offset;
}

/* Print insn arguments for 32/64-bit code.  */

static void
print_insn_args (const char *d, insn_t l, bfd_vma pc, disassemble_info *info)
{
  struct riscv_private_data *pd = info->private_data;
  int rs1 = (l >> OP_SH_RS1) & OP_MASK_RS1;
  int rd = (l >> OP_SH_RD) & OP_MASK_RD;
  int crd = (l >> OP_SH_CRD) & OP_MASK_CRD;
  fprintf_ftype print = info->fprintf_func;

  if (*d != '\0')
    print (info->stream, "\t");

  for (; *d != '\0'; d++)
    {
      switch (*d)
	{
	/* Xcustom */
	case '^':
	  switch (*++d)
	    {
	    case 'd':
	      print (info->stream, "%d", rd);
	      break;
	    case 's':
	      print (info->stream, "%d", rs1);
	      break;
	    case 't':
	      print (info->stream, "%d", (int) EXTRACT_OPERAND (RS2, l));
	      break;
	    case 'j':
	      print (info->stream, "%d", (int) EXTRACT_OPERAND (CUSTOM_IMM, l));
	      break;
	    }
	  break;

	case 'C': /* RVC */
	  switch (*++d)
	    {
	    case 's': /* RS1 x8-x15 */
	    case 'w': /* RS1 x8-x15 */
	      print (info->stream, "%s",
		     riscv_gpr_names[EXTRACT_OPERAND (CRS1S, l) + 8]);
	      break;
	    case 't': /* RS2 x8-x15 */
	    case 'x': /* RS2 x8-x15 */
	      print (info->stream, "%s",
		     riscv_gpr_names[EXTRACT_OPERAND (CRS2S, l) + 8]);
	      break;
	    case 'U': /* RS1, constrained to equal RD */
	      print (info->stream, "%s", riscv_gpr_names[crd]);
	      break;
	    case 'c': /* RS1, constrained to equal sp */
	      print (info->stream, "%s", riscv_gpr_names[X_SP]);
	      break;
	    case 'V': /* RS2 */
	      print (info->stream, "%s",
		     riscv_gpr_names[EXTRACT_OPERAND (CRS2, l)]);
	      break;
	    case 'd': /* RD, lisc */
	      print (info->stream, "%s",
		     riscv_gpr_names[EXTRACT_OPERAND (CRD, l)]);
	      break;
	    case 'i':
	      print (info->stream, "%d", (int)EXTRACT_RVC_SIMM3 (l));
	      break;
	    case 'j':
	      print (info->stream, "%d", (int)EXTRACT_RVC_IMM (l));
	      break;
	    case 'k':
	      print (info->stream, "%d", (int)EXTRACT_RVC_LW_IMM (l));
	      break;
	    case 'l':
	      print (info->stream, "%d", (int)EXTRACT_RVC_LD_IMM (l));
	      break;
	    case 'm':
	      print (info->stream, "%d", (int)EXTRACT_RVC_LWSP_IMM (l));
	      break;
	    case 'n':
	      print (info->stream, "%d", (int)EXTRACT_RVC_LDSP_IMM (l));
	      break;
	    case 'K':
	      print (info->stream, "%d", (int)EXTRACT_RVC_ADDI4SPN_IMM (l));
	      break;
	    case 'L':
	      print (info->stream, "%d", (int)EXTRACT_RVC_ADDI16SP_IMM (l));
	      break;
	    case 'M':
	      print (info->stream, "%d", (int)EXTRACT_RVC_SWSP_IMM (l));
	      break;
	    case 'N':
	      print (info->stream, "%d", (int)EXTRACT_RVC_SDSP_IMM (l));
	      break;
	    case 'p':
	      info->target = EXTRACT_RVC_B_IMM (l) + pc;
	      (*info->print_address_func) (info->target, info);
	      break;
	    case 'a':
	      info->target = EXTRACT_RVC_J_IMM (l) + pc;
	      (*info->print_address_func) (info->target, info);
	      break;
	    case 'u':
	      print (info->stream, "0x%x",
		     (int) (EXTRACT_RVC_IMM (l) & (RISCV_BIGIMM_REACH-1)));
	      break;
	    case '>':
	      print (info->stream, "0x%x", (int) EXTRACT_RVC_IMM (l) & 0x3f);
	      break;
	    case '<':
	      print (info->stream, "0x%x", (int) EXTRACT_RVC_IMM (l) & 0x1f);
	      break;
	    case 'T': /* floating-point RS2 */
	      print (info->stream, "%s",
		     riscv_fpr_names[EXTRACT_OPERAND (CRS2, l)]);
	      break;
	    case 'D': /* floating-point RS2 x8-x15 */
	      print (info->stream, "%s",
		     riscv_fpr_names[EXTRACT_OPERAND (CRS2S, l) + 8]);
	      break;
	    }
	  break;

	case ',':
        case '!':
	case '(':
	case ')':
	case '[':
	case ']':
	  print (info->stream, "%c", *d);
	  break;

	case '0':
	  /* Only print constant 0 if it is the last argument */
	  if (!d[1])
	    print (info->stream, "0");
	  break;

	case 'b':
          if (d[1]=='1') {
	     info->target = (EXTRACT_ITYPE_IMM (l)<<1) + pc; ++d;
	     (*info->print_address_func) (info->target, info);
	  } else if (d[1]=='2') {
	     info->target = (EXTRACT_I1TYPE_UIMM (l)<<1) + pc; ++d;
	     (*info->print_address_func) (info->target, info);
          } else if (d[1]=='3') {
	     print (info->stream, "%d", (int) EXTRACT_I1TYPE_UIMM (l)); ++d;
          } else if (d[1]=='5') {
	     print (info->stream, "%d", ((int) EXTRACT_I5TYPE_UIMM (l))&0x1F); ++d;
          } else if (d[1]=='I') {
	     print (info->stream, "%d", ((int) EXTRACT_I5_1_TYPE_IMM (l)<<27)>>27); ++d;
          } else if (d[1]=='i') {
	     print (info->stream, "%d", ((int) EXTRACT_I5_1_TYPE_UIMM (l))&0x1F); ++d;
	  } else if (d[1]=='s') {
	     print (info->stream, "%d", ((int) EXTRACT_I6TYPE_IMM (l)<<26)>>26); ++d;
	  } else if (d[1]=='u') {
	     print (info->stream, "%d", ((int) EXTRACT_I6TYPE_IMM (l))&0x1F); ++d;
	  } else if (d[1]=='U') {
	     print (info->stream, "%d", ((int) EXTRACT_I6TYPE_IMM (l))&0x0F); ++d;
	  } else if (d[1]=='f') {
	     print (info->stream, "%d", ((int) EXTRACT_I6TYPE_IMM (l))&0x01); ++d;
	  } else if (d[1]=='F') {
	     print (info->stream, "%d", ((int) EXTRACT_I6TYPE_IMM (l))&0x03); ++d;
	  } else (*info->print_address_func) (info->target, info);
          break;
	case 's':
	  print (info->stream, "%s", riscv_gpr_names[rs1]);
	  break;

        case 'r':
          print (info->stream, "%s",
                 riscv_gpr_names[EXTRACT_OPERAND (RS3I, l)]);
          break;


	case 't':
	  print (info->stream, "%s",
		 riscv_gpr_names[EXTRACT_OPERAND (RS2, l)]);
	  break;

	case 'u':
	  print (info->stream, "0x%x",
		 (unsigned) EXTRACT_UTYPE_IMM (l) >> RISCV_IMM_BITS);
	  break;

	case 'm':
	  arg_print (info, EXTRACT_OPERAND (RM, l),
		     riscv_rm, ARRAY_SIZE (riscv_rm));
	  break;

	case 'P':
	  arg_print (info, EXTRACT_OPERAND (PRED, l),
		     riscv_pred_succ, ARRAY_SIZE (riscv_pred_succ));
	  break;

	case 'Q':
	  arg_print (info, EXTRACT_OPERAND (SUCC, l),
		     riscv_pred_succ, ARRAY_SIZE (riscv_pred_succ));
	  break;

	case 'o':
	  maybe_print_address (pd, rs1, EXTRACT_ITYPE_IMM (l));
	case 'j':
	  if (d[1]=='i') {
             ++d;
	     print (info->stream, "%d", (int) EXTRACT_ITYPE_IMM (l));
	     break;
	  }
	  if ((l & MASK_ADDI) == MATCH_ADDI || (l & MASK_JALR) == MATCH_JALR)
	    maybe_print_address (pd, rs1, EXTRACT_ITYPE_IMM (l));
	  print (info->stream, "%d", (int) EXTRACT_ITYPE_IMM (l));
	  break;

	case 'q':
	  maybe_print_address (pd, rs1, EXTRACT_STYPE_IMM (l));
	  print (info->stream, "%d", (int) EXTRACT_STYPE_IMM (l));
	  break;

	case 'a':
	  info->target = EXTRACT_UJTYPE_IMM (l) + pc;
	  (*info->print_address_func) (info->target, info);
	  break;

	case 'p':
	  info->target = EXTRACT_SBTYPE_IMM (l) + pc;
	  (*info->print_address_func) (info->target, info);
	  break;

	case 'd':
	  if ((l & MASK_AUIPC) == MATCH_AUIPC)
	    pd->hi_addr[rd] = pc + EXTRACT_UTYPE_IMM (l);
	  else if ((l & MASK_LUI) == MATCH_LUI)
	    pd->hi_addr[rd] = EXTRACT_UTYPE_IMM (l);
	  else if ((l & MASK_C_LUI) == MATCH_C_LUI)
	    pd->hi_addr[rd] = EXTRACT_RVC_LUI_IMM (l);
          if (d[1]=='i') {
             ++d;
             print (info->stream, "x%d", (int) rd);
          } else print (info->stream, "%s", riscv_gpr_names[rd]);
	  break;

	case 'z':
	  print (info->stream, "%s", riscv_gpr_names[0]);
	  break;

	case '>':
	  print (info->stream, "0x%x", (int) EXTRACT_OPERAND (SHAMT, l));
	  break;

	case '<':
	  print (info->stream, "0x%x", (int) EXTRACT_OPERAND (SHAMTW, l));
	  break;

	case 'S':
	case 'U':
	  print (info->stream, "%s", riscv_fpr_names[rs1]);
	  break;

	case 'T':
	  print (info->stream, "%s", riscv_fpr_names[EXTRACT_OPERAND (RS2, l)]);
	  break;

	case 'D':
	  print (info->stream, "%s", riscv_fpr_names[rd]);
	  break;

	case 'R':
	  print (info->stream, "%s", riscv_fpr_names[EXTRACT_OPERAND (RS3, l)]);
	  break;

	case 'E':
	  {
	    const char* csr_name = NULL;
	    unsigned int csr = EXTRACT_OPERAND (CSR, l);
	    switch (csr)
	      {
	      #define DECLARE_CSR(name, num) case num: csr_name = #name; break;
	      #include "opcode/riscv-opc.h"
	      #undef DECLARE_CSR
	      }
	    if (csr_name)
	      print (info->stream, "%s", csr_name);
	    else
	      print (info->stream, "0x%x", csr);
	    break;
	  }

	case 'Z':
	  print (info->stream, "%d", rs1);
	  break;

	default:
	  /* xgettext:c-format */
	  print (info->stream, _("# internal error, undefined modifier (%c)"),
		 *d);
	  return;
	}
    }
}

static bfd_boolean
riscv_subset_supports (const char *feature)
{
  struct riscv_subset *s;
  char *p;
  // unsigned xlen_required;

  if (!riscv_subsets) return TRUE;
  (void) strtoul (feature, &p, 10);

  for (s = riscv_subsets; s != NULL; s = s->next)
    if (strcasecmp (s->name, p) == 0)
      /* FIXME: once we support version numbers:
         return major == s->version_major && minor <= s->version_minor; */
      return TRUE;

  return FALSE;
}

/* Print the RISC-V instruction at address MEMADDR in debugged memory,
   on using INFO.  Returns length of the instruction, in bytes.
   BIGENDIAN must be 1 if this is big-endian code, 0 if
   this is little-endian code.  */

static int
riscv_disassemble_insn (bfd_vma memaddr, insn_t word, disassemble_info *info)
{
  const struct riscv_opcode *op;
  static bfd_boolean init = 0;
  static const struct riscv_opcode *riscv_hash[OP_MASK_OP + 1];
  struct riscv_private_data *pd;
  int insnlen;

#define OP_HASH_IDX(i) ((i) & (riscv_insn_length (i) == 2 ? 0x3 : OP_MASK_OP))

  /* Build a hash table to shorten the search time.  */
  if (! init)
    {
      for (op = riscv_opcodes; op < &riscv_opcodes[NUMOPCODES]; op++)
	if (riscv_subset_supports (op->subset)) {
		if (!riscv_hash[OP_HASH_IDX (op->match)])
	  		riscv_hash[OP_HASH_IDX (op->match)] = op;
	}

      init = 1;
    }

  if (info->private_data == NULL)
    {
      int i;

      pd = info->private_data = xcalloc (1, sizeof (struct riscv_private_data));
      pd->gp = -1;
      pd->print_addr = -1;
      for (i = 0; i < (int) ARRAY_SIZE (pd->hi_addr); i++)
	pd->hi_addr[i] = -1;

      for (i = 0; i < info->symtab_size; i++)
	if (strcmp (bfd_asymbol_name (info->symtab[i]), "_gp") == 0)
	  pd->gp = bfd_asymbol_value (info->symtab[i]);
    }
  else
    pd = info->private_data;

  insnlen = riscv_insn_length (word);

  info->bytes_per_chunk = insnlen % 4 == 0 ? 4 : 2;
  info->bytes_per_line = 8;
  info->display_endian = info->endian;
  info->insn_info_valid = 1;
  info->branch_delay_insns = 0;
  info->data_size = 0;
  info->insn_type = dis_nonbranch;
  info->target = 0;
  info->target2 = 0;

  op = riscv_hash[OP_HASH_IDX (word)];
  if (op != NULL)
    {
      int xlen = 0;

      /* The incoming section might not always be complete.  */
      if (info->section != NULL)
	{
	  Elf_Internal_Ehdr *ehdr = elf_elfheader (info->section->owner);
	  xlen = ehdr->e_ident[EI_CLASS] == ELFCLASS64 ? 64 : 32;
	}

      for (; op < &riscv_opcodes[NUMOPCODES]; op++)
	{
	  /* Is the isa subset of this instruction supported? */
	  if (!riscv_subset_supports (op->subset)) 
 	    continue;
	  /* Does the opcode match?  */
	  if (! (op->match_func) (op, word))
	    continue;
	  /* Is this a pseudo-instruction and may we print it as such?  */
	  if (no_aliases && (op->pinfo & INSN_ALIAS))
	    continue;
	  /* Is this instruction restricted to a certain value of XLEN?  */
	  if (isdigit (op->subset[0]) && atoi (op->subset) != xlen)
	    continue;

	  /* It's a match.  */
	  (*info->fprintf_func) (info->stream, "%s", op->name);
	  print_insn_args (op->args, word, memaddr, info);

	  /* Try to disassemble multi-instruction addressing sequences.  */
	  if (pd->print_addr != (bfd_vma)-1)
	    {
	      info->target = pd->print_addr;
	      (*info->fprintf_func) (info->stream, " # ");
	      (*info->print_address_func) (info->target, info);
	      pd->print_addr = -1;
	    }

	  return insnlen;
	}
    }

  /* We did not find a match, so just print the instruction bits.  */
  info->insn_type = dis_noninsn;
  (*info->fprintf_func) (info->stream, "0x%llx", (unsigned long long)word);
  return insnlen;
}

int
print_insn_riscv (bfd_vma memaddr, struct disassemble_info *info)
{
  bfd_byte packet[2];
  insn_t insn = 0;
  bfd_vma n;
  int status;

  if (info->disassembler_options != NULL)
    {
      parse_riscv_dis_options (info->disassembler_options);
      /* Avoid repeatedly parsing the options.  */
      info->disassembler_options = NULL;
    }
  else if (riscv_gpr_names == NULL)
    set_default_riscv_dis_options ();

  /* Instructions are a sequence of 2-byte packets in little-endian order.  */
  for (n = 0; n < sizeof (insn) && n < riscv_insn_length (insn); n += 2)
    {
      status = (*info->read_memory_func) (memaddr + n, packet, 2, info);
      if (status != 0)
	{
	  /* Don't fail just because we fell off the end.  */
	  if (n > 0)
	    break;
	  (*info->memory_error_func) (status, memaddr, info);
	  return status;
	}

      insn |= ((insn_t) bfd_getl16 (packet)) << (8 * n);
    }

  return riscv_disassemble_insn (memaddr, insn, info);
}

void
print_riscv_disassembler_options (FILE *stream)
{
  fprintf (stream, _("\n\
The following RISC-V-specific disassembler options are supported for use\n\
with the -M switch (multiple options should be separated by commas):\n"));

  fprintf (stream, _("\n\
  numeric           Print numeric register names, rather than ABI names.\n"));

  fprintf (stream, _("\n\
  no-aliases        Disassemble only into canonical instructions, rather\n\
                    than into pseudoinstructions.\n"));
  fprintf (stream, _("\n\
  march=ISA string Uses string to select riscv ISA subsets.\n"));

  fprintf (stream, _("\n"));
}
