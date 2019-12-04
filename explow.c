/* Subroutines for manipulating rtx's in semantically interesting ways.
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


#include "config.h"
#include "rtl.h"
#include "tree.h"
#include "expr.h"

/* Return an rtx for the sum of X and the integer C.  */

rtx
plus_constant (x, c)
     register rtx x;
     register int c;
{
  register RTX_CODE code = GET_CODE (x);
  register enum machine_mode mode = GET_MODE (x);
  int all_constant = 0;

  if (c == 0)
    return x;

  if (code == CONST_INT)
    return gen_rtx (CONST_INT, VOIDmode, (INTVAL (x) + c));

  /* If adding to something entirely constant, set a flag
     so that we can add a CONST around the result.  */
  if (code == CONST)
    {
      x = XEXP (x, 0);
      all_constant = 1;
    }
  else if (code == SYMBOL_REF || code == LABEL_REF)
    all_constant = 1;

  /* The interesting case is adding the integer to a sum.
     Look for constant term in the sum and combine
     with C.  For an integer constant term, we make a combined
     integer.  For a constant term that is not an explicit integer,
     we cannot really combine, but group them together anyway.  */

  if (GET_CODE (x) == PLUS)
    {
      if (GET_CODE (XEXP (x, 0)) == CONST_INT)
	{
	  c += INTVAL (XEXP (x, 0));
	  x = XEXP (x, 1);
	}
      else if (GET_CODE (XEXP (x, 1)) == CONST_INT)
	{
	  c += INTVAL (XEXP (x, 1));
	  x = XEXP (x, 0);
	}
      else if (GET_CODE (XEXP (x, 0)) == CONST)
	{
	  return gen_rtx (PLUS, mode,
			  plus_constant (XEXP (x, 0), c),
			  XEXP (x, 1));
	}
      else if (GET_CODE (XEXP (x, 1)) == CONST)
	{
	  return gen_rtx (PLUS, mode,
			  XEXP (x, 0),
			  plus_constant (XEXP (x, 1), c));
	}
#ifdef OLD_INDEXING
      /* Detect adding a constant to an indexed address
	 of the form (PLUS (MULT (REG) (CONST)) regs-and-constants).
	 Keep the (MULT ...) at the top level of addition so that
	 the result is still suitable for indexing and constants
	 are combined.  */
      else if (GET_CODE (XEXP (x, 0)) == MULT)
	{
	  return gen_rtx (PLUS, mode, XEXP (x, 0),
			  plus_constant (XEXP (x, 1), c));
	}
      else if (GET_CODE (XEXP (x, 1)) == MULT)
	{
	  return gen_rtx (PLUS, mode, plus_constant (XEXP (x, 0), c),
			  XEXP (x, 1));
	}
#endif
    }
  if (c != 0)
    x = gen_rtx (PLUS, mode, x, gen_rtx (CONST_INT, VOIDmode, c));

  if (all_constant)
    return gen_rtx (CONST, VOIDmode, x);
  else
    return x;
}

/* If X is a sum, return a new sum like X but lacking any constant terms.
   Add all the removed constant terms into *CONSTPTR.
   X itself is not altered.  The result != X if and only if
   it is not isomorphic to X.  */

rtx
eliminate_constant_term (x, constptr)
     rtx x;
     int *constptr;
{
  int c;
  register rtx x0, x1;

  if (GET_CODE (x) != PLUS)
    return x;

  /* First handle constants appearing at this level explicitly.  */
  if (GET_CODE (XEXP (x, 0)) == CONST_INT)
    {
      *constptr += INTVAL (XEXP (x, 0));
      return eliminate_constant_term (XEXP (x, 1), constptr);
    }

  if (GET_CODE (XEXP (x, 1)) == CONST_INT)
    {
      *constptr += INTVAL (XEXP (x, 1));
      return eliminate_constant_term (XEXP (x, 0), constptr);
    }

  c = 0;
  x0 = eliminate_constant_term (XEXP (x, 0), &c);
  x1 = eliminate_constant_term (XEXP (x, 1), &c);
  if (x1 != XEXP (x, 1) || x0 != XEXP (x, 0))
    {
      *constptr += c;
      return gen_rtx (PLUS, GET_MODE (x), x0, x1);
    }
  return x;
}

/* Return an rtx for the size in bytes of the value of EXP.  */

rtx
expr_size (exp)
     tree exp;
{
  return expand_expr (size_in_bytes (TREE_TYPE (exp)), 0, SImode, 0);
}

/* Not yet really written since C does not need it.  */

rtx
lookup_static_chain ()
{
  abort ();
}

/* Return a copy of X in which all memory references
   and all constants that involve symbol refs
   have been replaced with new temporary registers.
   Also emit code to load the memory locations and constants
   into those registers.

   If X contains no such constants or memory references,
   X itself (not a copy) is returned.

   X may contain no arithmetic except addition, subtraction and multiplication.
   Values returned by expand_expr with 1 for sum_ok fit this constraint.  */

static rtx
break_out_memory_refs (x)
     register rtx x;
{
  if (GET_CODE (x) == MEM || GET_CODE (x) == CONST
      || GET_CODE (x) == SYMBOL_REF)
    {
      register rtx temp = gen_reg_rtx (Pmode);
      if (GET_MODE (x) != Pmode && GET_MODE (x) != VOIDmode)
	abort ();
      emit_move_insn (temp, x);
      /* Let optimizers know that TEMP's value never changes
	 and that X can be substituted for it.  */
      if (GET_CODE (x) != MEM)
	REG_NOTES (get_last_insn ()) = gen_rtx (EXPR_LIST, REG_CONST, temp, 0);
      mark_reg_pointer (temp);
      x = temp;
    }
  else if (GET_CODE (x) == PLUS || GET_CODE (x) == MINUS
	   || GET_CODE (x) == MULT)
    {
      register rtx op0 = break_out_memory_refs (XEXP (x, 0));
      register rtx op1 = break_out_memory_refs (XEXP (x, 1));
      if (op0 != XEXP (x, 0) || op1 != XEXP (x, 1))
	x = gen_rtx (GET_CODE (x), Pmode, op0, op1);
    }
  return x;
}

/* Given a memory address or facsimile X, construct a new address,
   currently equivalent, that is stable: future stores won't change it.

   X must be composed of constants, register and memory references
   combined with addition, subtraction and multiplication:
   in other words, just what you can get from expand_expr if sum_ok is 1.

   Works by making copies of all regs and memory locations used
   by X and combining them the same way X does.
   You could also stabilize the reference to this address
   by copying the address to a register with copy_to_reg;
   but then you wouldn't get indexed addressing in the reference.  */

rtx
copy_all_regs (x)
     register rtx x;
{
  if (GET_CODE (x) == MEM || GET_CODE (x) == REG)
    {
      x = copy_to_reg (x);
    }
  else if (GET_CODE (x) == PLUS || GET_CODE (x) == MINUS
	   || GET_CODE (x) == MULT)
    {
      register rtx op0 = copy_all_regs (XEXP (x, 0));
      register rtx op1 = copy_all_regs (XEXP (x, 1));
      if (op0 != XEXP (x, 0) || op1 != XEXP (x, 1))
	x = gen_rtx (GET_CODE (x), Pmode, op0, op1);
    }
  return x;
}

/* Return something equivalent to X but valid as a memory address
   for something of mode MODE.  When X is not itself valid, this
   works by copying X or subexpressions of it into registers.  */

rtx
memory_address (mode, x)
     enum machine_mode mode;
     register rtx x;
{
  register rtx tem, oldx;

  /* By passing constant addresses thru registers
     we get a chance to cse them.  */
  if (! cse_not_expected && CONSTANT_ADDRESS_P (x))
    {
      tem = copy_to_suggested_reg (x, gen_reg_rtx (Pmode));
      REG_NOTES (get_last_insn ()) = gen_rtx (EXPR_LIST, REG_CONST, tem, 0);
      return tem;
    }

  /* Accept a QUEUED that refers to a REG
     even though that isn't a valid address.
     On attempting to put this in an insn we will call protect_from_queue
     which will turn it into a REG, which is valid.  */
  if (GET_CODE (x) == QUEUED
      && GET_CODE (QUEUED_VAR (x)) == REG)
    return x;

  /* We get better cse by rejecting indirect addressing at this stage.
     Let the combiner create indirect addresses where appropriate.
     For now, generate the code so that the subexpressions useful to share
     are visible.  But not if cse won't be done!  */
  oldx = x;
  if (! cse_not_expected && GET_CODE (x) != REG)
    x = break_out_memory_refs (x);

  /* At this point, any valid address is accepted.  */
  GO_IF_LEGITIMATE_ADDRESS (mode, x, win);

  /* If it was valid before but breaking out memory refs invalidated it,
     use it the old way.  */
  GO_IF_LEGITIMATE_ADDRESS (mode, oldx, win2);

  /* Perform machine-dependent transformations on X
     in certain cases.  This is not necessary since the code
     below can handle all possible cases, but machine-dependent
     transformations can make better code.  */
  LEGITIMIZE_ADDRESS (x, oldx, mode, win);

  /* PLUS and MULT can appear in special ways
     as the result of attempts to make an address usable for indexing.
     Usually they are dealt with by calling force_operand, below.
     But a sum containing constant terms is special
     if removing them makes the sum a valid address:
     then we generate that address in a register
     and index off of it.  We do this because it often makes
     shorter code, and because the addresses thus generated
     in registers often become common subexpressions.  */
  if (GET_CODE (x) == PLUS)
    {
      int constant_term = 0;
      rtx y = eliminate_constant_term (x, &constant_term);
      if (constant_term != 0)
	GO_IF_LEGITIMATE_ADDRESS (mode, y, win1);
      return force_operand (x, 0);

    win1:
      return plus_constant (copy_to_reg (y), constant_term);
    }
  if (GET_CODE (x) == MULT || GET_CODE (x) == MINUS)
    return force_operand (x, 0);

  /* Last resort: copy the value to a register, since
     the register is a valid address.  */
  return copy_to_reg (x);

 win2:
  x = oldx;
 win:
  if (force_addr && GET_CODE (x) != REG)
    return copy_to_reg (x);
  return x;
}

/* Return 1 if X and Y are identical-looking rtx's.
   This is the Lisp function EQUAL for rtx arguments.  */

int
rtx_equal_p (x, y)
     rtx x, y;
{
  register int i;
  register int hash = 0;
  register RTX_CODE code = GET_CODE (x);
  register char *fmt;

  if (x == y)
    return 1;

  /* Rtx's of different codes cannot be equal.  */
  if (code != GET_CODE (y))
    return 0;

  /* These three types of rtx's can be compared nonrecursively.  */
  if (code == REG)
    return (REGNO (x) == REGNO (y));
  if (code == LABEL_REF)
    return XEXP (x, 0) == XEXP (y, 0);
  if (code == SYMBOL_REF)
    return XSTR (x, 0) == XSTR (y, 0);

  /* (MULT:SI x y) and (MULT:HI x y) are NOT equivalent.  */

  if (GET_MODE (x) != GET_MODE (y))
    return 0;

  /* Compare the elements.  If any pair of corresponding elements
     fail to match, return 0 for the whole things.  */

  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      switch (fmt[i])
	{
	case 'i':
	  if (XINT (x, i) != XINT (y, i))
	    return 0;
	  break;

	case 'e':
	  if (rtx_equal_p (XEXP (x, i), XEXP (y, i)) == 0)
	    return 0;
	  break;

	case '0':
	  break;

	  /* It is believed that rtx's at this level will never
	     contain anything but integers and other rtx's,
	     except for within LABEL_REFs and SYMBOL_REFs.  */
	default:
	  abort ();
	}
    }
  return 1;
}

/* Return a modified copy of X with its memory address copied
   into a temporary register to protect it from side effects.
   If X is not a MEM, it is returned unchanged (and not copied).
   Perhaps even if it is a MEM, if there is no need to change it.  */

rtx
stabilize (x)
     rtx x;
{
  register rtx addr;
  if (GET_CODE (x) != MEM)
    return x;
  addr = XEXP (x, 0);
  if (rtx_varies_p (addr))
    {
      rtx temp = copy_all_regs (addr);
      rtx mem;
      if (GET_CODE (temp) != REG)
	temp = copy_to_reg (temp);
      mem = gen_rtx (MEM, GET_MODE (x), temp);
      /* Mark returned memref with in_struct
	 if it's in an array or structure. */
      if (GET_CODE (addr) == PLUS || x->in_struct)
	mem->in_struct = 1;
      return mem;
    }
  return x;
}

/* Copy the value or contents of X to a new temp reg and return that reg.  */

rtx
copy_to_reg (x)
     rtx x;
{
  register rtx temp = gen_reg_rtx (GET_MODE (x));
  emit_move_insn (temp, x);
  return temp;
}

/* If X is a memory ref, copy its contents to a new temp reg and return
   that reg.  Otherwise, return X.  */

rtx
force_not_mem (x)
     rtx x;
{
  register rtx temp;
  if (GET_CODE (x) != MEM)
    return x;
  temp = gen_reg_rtx (GET_MODE (x));
  emit_move_insn (temp, x);
  return temp;
}

/* Copy X to TARGET (if it's nonzero and a reg)
   or to a new temp reg and return that reg.  */

rtx
copy_to_suggested_reg (x, target)
     rtx x, target;
{
  register rtx temp;
  if (target && GET_CODE (target) == REG)
    temp = target;
  else
    temp = gen_reg_rtx (GET_MODE (x));
  emit_move_insn (temp, x);
  return temp;
}

/* Adjust the stack pointer by ADJUST (an rtx for a number of bytes).
   This pops when ADJUST is positive.  ADJUST need not be constant.  */

void
adjust_stack (adjust)
     rtx adjust;
{
  adjust = protect_from_queue (adjust, 0);

#ifdef STACK_GROWS_DOWNWARD
  emit_insn (gen_add2_insn (gen_rtx (REG, SImode, STACK_POINTER_REGNUM),
			    adjust));
#else
  emit_insn (gen_sub2_insn (gen_rtx (REG, SImode, STACK_POINTER_REGNUM),
			    adjust));
#endif
}

/* Adjust the stack pointer by minus ADJUST (an rtx for a number of bytes).
   This pushes when ADJUST is positive.  ADJUST need not be constant.  */

void
anti_adjust_stack (adjust)
     rtx adjust;
{
  adjust = protect_from_queue (adjust, 0);

#ifdef STACK_GROWS_DOWNWARD
  emit_insn (gen_sub2_insn (gen_rtx (REG, SImode, STACK_POINTER_REGNUM),
			    adjust));
#else
  emit_insn (gen_add2_insn (gen_rtx (REG, SImode, STACK_POINTER_REGNUM),
			    adjust));
#endif
}

/* Return a pseudo reg representing the value returned by
   a function call that was just emitted.  */

rtx
function_value (mode)
     enum machine_mode mode;
{
  /* Copy register 0 in case the value we return
     will not get used until after another function call happens.  */
  return copy_to_reg (gen_rtx (REG, mode, FUNCTION_VALUE_REGNUM));
}

/* Return a hard reg representing the value returned by
   a function call that was just emitted.  */

rtx
hard_function_value (mode)
     enum machine_mode mode;
{
  return gen_rtx (REG, mode, FUNCTION_VALUE_REGNUM);
}

/* Return a pseudo reg representing the value returned by
   a function call that was just emitted.  */

void
copy_function_value (reg)
     rtx reg;
{
  emit_move_insn (reg, gen_rtx (REG, GET_MODE (reg), FUNCTION_VALUE_REGNUM));
}
