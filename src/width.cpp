/**
 * @file width.cpp
 * Limits line width.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "width.h"
#include "uncrustify_types.h"
#include "chunk_list.h"
#include "prototypes.h"
#include "uncrustify.h"
#include "indent.h"
#include "newlines.h"
#include <cstdlib>
#include <limits>
#include <iostream>

/**
 * abbreviations used:
 * - fparen = function parenthesis
 */

struct cw_entry
{
   chunk_t *pc;
   size_t  pri;
};


struct token_pri
{
   c_token_t tok;
   size_t    pri;
};


static_inline bool is_past_width(chunk_t *pc);


//! Split right after the chunk
static void split_before_chunk(chunk_t *pc);


static size_t get_split_pri(c_token_t tok);


/**
 * Checks to see if pc is a better spot to split.
 * This should only be called going BACKWARDS (ie prev)
 * A lower level wins
 *
 * Splitting Preference:
 *  - semicolon
 *  - comma
 *  - boolean op
 *  - comparison
 *  - arithmetic op
 *  - assignment
 *  - concatenated strings
 *  - ? :
 *  - function open paren not followed by close paren
 */
static void try_split_here(cw_entry &ent, chunk_t *pc);


/**
 * Scan backwards to find the most appropriate spot to split the line
 * and insert a newline.
 *
 * See if this needs special function handling.
 * Scan backwards and find the best token for the split.
 *
 * @param start The first chunk that exceeded the limit
 */
static bool split_line(chunk_t * &pc);


/**
 * Figures out where to split a function def/proto/call
 *
 * For function prototypes and definition. Also function calls where
 * level == brace_level:
 *   - find the open function parenthesis
 *     + if it doesn't have a newline right after it
 *       * see if all parameters will fit individually after the paren
 *       * if not, throw a newline after the open paren & return
 *   - scan backwards to the open fparen or comma
 *     + if there isn't a newline after that item, add one & return
 *     + otherwise, add a newline before the start token
 *
 * @param start   the offending token
 * @return        the token that should have a newline
 *                inserted before it
 */
static void split_fcn_params(chunk_t * &start);


/**
 * Splits the parameters at every comma that is at the fparen level.
 *
 * @param start   the offending token
 */
static void split_fcn_params_full(chunk_t *start);


/**
 * A for statement is too long.
 * Step backwards and forwards to find the semicolons
 * Try splitting at the semicolons first.
 * If that doesn't work, then look for a comma at paren level.
 * If that doesn't work, then look for an assignment at paren level.
 * If that doesn't work, then give up.
 */
static void split_for_stmt(chunk_t *start);


static_inline bool is_past_width(chunk_t *pc)
{
   // allow char to sit at last column by subtracting 1
   return((pc->column + pc->len() - 1) > cpd.settings[UO_code_width].u);
}


static void split_before_chunk(chunk_t *pc)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "  %s: %s\n", __func__, pc->text());

   if (  !chunk_is_newline(pc)
      && !chunk_is_newline(chunk_get_prev(pc)))
   {
      newline_add_before(pc);
      // reindent needs to include the indent_continue value and was off by one
      reindent_line(pc, pc->brace_level * cpd.settings[UO_indent_columns].u +
                    abs(cpd.settings[UO_indent_continue].n) + 1);
      cpd.changes++;
   }
}


void do_code_width(void)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "%s\n", __func__);

   for (chunk_t *pc = chunk_get_head(); pc != nullptr; pc = chunk_get_next(pc))
   {
      if (  !chunk_is_newline(pc)
         && !chunk_is_comment(pc)
         && (pc->type != CT_SPACE)
         && is_past_width(pc))
      {
         bool split_OK = split_line(pc);
         if (split_OK)
         {
            LOG_FMT(LSPLIT, "%s(%d): on orig_line=%zu, orig_col=%zu, for %s\n",
                    __func__, __LINE__, pc->orig_line, pc->orig_col, pc->text());
         }
         else
         {
            LOG_FMT(LSPLIT, "%s(%d): Bailed on orig_line=%zu, orig_col=%zu, for %s\n",
                    __func__, __LINE__, pc->orig_line, pc->orig_col, pc->text());
            break;
         }
      }
   }
}


static const token_pri pri_table[] =
{
   { CT_SEMICOLON,    1 },
   { CT_COMMA,        2 },
   { CT_BOOL,         3 },
   { CT_COMPARE,      4 },
   { CT_ARITH,        5 },
   { CT_CARET,        6 },
   { CT_ASSIGN,       7 },
   { CT_STRING,       8 },
   { CT_FOR_COLON,    9 },
   //{ CT_DC_MEMBER, 10 },
   //{ CT_MEMBER,    10 },
   { CT_QUESTION,    20 }, // allow break in ? : for ls_code_width
   { CT_COND_COLON,  20 },
   { CT_FPAREN_OPEN, 21 }, // break after function open paren not followed by close paren
   { CT_QUALIFIER,   25 },
   { CT_CLASS,       25 },
   { CT_STRUCT,      25 },
   { CT_TYPE,        25 },
   { CT_TYPENAME,    25 },
   { CT_VOLATILE,    25 },
};


static size_t get_split_pri(c_token_t tok)
{
   for (auto token : pri_table)
   {
      if (token.tok == tok)
      {
         return(token.pri);
      }
   }
   return(0);
}


static void try_split_here(cw_entry &ent, chunk_t *pc)
{
   LOG_FUNC_ENTRY();

   LOG_FMT(LSPLIT, "%s(%d): at %s, orig_col=%zu\n", __func__, __LINE__, pc->text(), pc->orig_col);
   size_t pc_pri = get_split_pri(pc->type);
   LOG_FMT(LSPLIT, "%s(%d): pc_pri=%zu\n", __func__, __LINE__, pc_pri);
   if (pc_pri == 0)
   {
      LOG_FMT(LSPLIT, "%s(%d): pc_pri is 0, return\n", __func__, __LINE__);
      return;
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // Can't split after a newline
   chunk_t *prev = chunk_get_prev(pc);
   if (  (prev == nullptr)
      || (chunk_is_newline(prev) && (pc->type != CT_STRING)))
   {
      LOG_FMT(LSPLIT, "%s(%d): Can't split after a newline, orig_line=%zu, return\n",
              __func__, __LINE__, (prev == nullptr ? std::numeric_limits<short>::max() : prev->orig_line));
      return;
   }
   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // Can't split a function without arguments
   if (pc->type == CT_FPAREN_OPEN)
   {
      chunk_t *next = chunk_get_next(pc);
      if (next->type == CT_FPAREN_CLOSE)
      {
         LOG_FMT(LSPLIT, "%s(%d): Can't split a function without arguments, return\n", __func__, __LINE__);
         return;
      }
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // Only split concatenated strings
   if (pc->type == CT_STRING)
   {
      chunk_t *next = chunk_get_next(pc);
      if (next->type != CT_STRING)
      {
         LOG_FMT(LSPLIT, "%s(%d): Only split concatenated strings, return\n", __func__, __LINE__);
         return;
      }
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // keep common groupings unless ls_code_width
   if (!cpd.settings[UO_ls_code_width].b && (pc_pri >= 20))
   {
      LOG_FMT(LSPLIT, "%s(%d): keep common groupings unless ls_code_width, return\n", __func__, __LINE__);
      return;
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // don't break after last term of a qualified type
   if (pc_pri == 25)
   {
      chunk_t *next = chunk_get_next(pc);
      if ((next->type != CT_WORD) && (get_split_pri(next->type) != 25))
      {
         LOG_FMT(LSPLIT, "%s(%d): don't break after last term of a qualified type, return\n", __func__, __LINE__);
         return;
      }
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // Check levels first
   bool change = false;
   if ((ent.pc == nullptr) || (pc->level < ent.pc->level))
   {
      LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
      change = true;
   }
   else
   {
      if ((pc->level >= ent.pc->level) && (pc_pri < ent.pri))
      {
         LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
         change = true;
      }
   }

   LOG_FMT(LSPLIT, "%s(%d): change is %s\n", __func__, __LINE__, change ? "TRUE" : "FALSE");
   if (change)
   {
      LOG_FMT(LSPLIT, "%s(%d): do the change\n", __func__, __LINE__);
      ent.pc  = pc;
      ent.pri = pc_pri;
   }
} // try_split_here


static bool split_line(chunk_t * &start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "%s(%d): start->flags ", __func__, __LINE__);
   log_pcf_flags(LSPLIT, start->flags);
   LOG_FMT(LSPLIT, "%s(%d): orig_line=%zu, orig_col=%zu, token: '%s', type=%s,\n",
           __func__, __LINE__, start->orig_line, start->column, start->text(),
           get_token_name(start->type));
   LOG_FMT(LSPLIT, "   parent_type %s, (PCF_IN_FCN_DEF is %s), (PCF_IN_FCN_CALL is %s),",
           get_token_name(start->parent_type),
           ((start->flags & (PCF_IN_FCN_DEF)) != 0) ? "TRUE" : "FALSE",
           ((start->flags & (PCF_IN_FCN_CALL)) != 0) ? "TRUE" : "FALSE");
#ifdef DEBUG
   LOG_FMT(LSPLIT, "\n");
#endif // DEBUG

   // break at maximum line length if ls_code_width is true
   if (start->flags & PCF_ONE_LINER)
   {
      LOG_FMT(LSPLIT, " ** ONCE LINER SPLIT **\n");
      undo_one_liner(start);
      newlines_cleanup_braces(false);
      return(false);
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   if (cpd.settings[UO_ls_code_width].b)
   {
   }
   // Check to see if we are in a for statement
   else if (start->flags & PCF_IN_FOR)
   {
      LOG_FMT(LSPLIT, " ** FOR SPLIT **\n");
      split_for_stmt(start);
      if (!is_past_width(start))
      {
         return(true);
      }
      LOG_FMT(LSPLIT, "%s(%d): for split didn't work\n", __func__, __LINE__);
   }

   /*
    * If this is in a function call or prototype, split on commas or right
    * after the open parenthesis
    */
   else if (  (start->flags & PCF_IN_FCN_DEF)
           || (start->flags & PCF_IN_FCN_CALL)
           || start->parent_type == CT_FUNC_PROTO)  // Issue #1169)
   {
      LOG_FMT(LSPLIT, " ** FUNC SPLIT **\n");

      if (cpd.settings[UO_ls_func_split_full].b)
      {
         split_fcn_params_full(start);
         if (!is_past_width(start))
         {
            return(true);
         }
      }
      split_fcn_params(start);
      return(true);
   }

   LOG_FMT(LSPLIT, "%s(%d):\n", __func__, __LINE__);
   // Try to find the best spot to split the line
   cw_entry ent;

   memset(&ent, 0, sizeof(ent));
   chunk_t *pc = start;
   chunk_t *prev;

   while (((pc = chunk_get_prev(pc)) != nullptr) && !chunk_is_newline(pc))
   {
      LOG_FMT(LSPLIT, "%s(%d): at %s, orig_line=%zu, orig_col=%zu\n",
              __func__, __LINE__, pc->text(), pc->orig_line, pc->orig_col);
      if (pc->type != CT_SPACE)
      {
         try_split_here(ent, pc);
         // break at maximum line length
         if ((ent.pc != nullptr) && (cpd.settings[UO_ls_code_width].b))
         {
            break;
         }
      }
   }

   if (ent.pc == nullptr)
   {
      LOG_FMT(LSPLIT, "\n%s(%d):    TRY_SPLIT yielded NO SOLUTION for orig_line %zu at %s [%s]\n",
              __func__, __LINE__, start->orig_line, start->text(), get_token_name(start->type));
   }
   else
   {
      LOG_FMT(LSPLIT, "\n%s(%d):    TRY_SPLIT yielded '%s' [%s] on orig_line %zu\n",
              __func__, __LINE__, ent.pc->text(), get_token_name(ent.pc->type), ent.pc->orig_line);
      LOG_FMT(LSPLIT, "%s(%d): ent at %s, orig_col=%zu\n",
              __func__, __LINE__, ent.pc->text(), ent.pc->orig_col);
   }

   // Break before the token instead of after it according to the pos_xxx rules
   if (ent.pc == nullptr)
   {
      pc = nullptr;
   }
   else
   {
      if (  (  (  chunk_is_token(ent.pc, CT_ARITH)
               || chunk_is_token(ent.pc, CT_CARET))
            && (cpd.settings[UO_pos_arith].tp & TP_LEAD))
         || (  chunk_is_token(ent.pc, CT_ASSIGN)
            && (cpd.settings[UO_pos_assign].tp & TP_LEAD))
         || (  chunk_is_token(ent.pc, CT_COMPARE)
            && (cpd.settings[UO_pos_compare].tp & TP_LEAD))
         || (  (  chunk_is_token(ent.pc, CT_COND_COLON)
               || chunk_is_token(ent.pc, CT_QUESTION))
            && (cpd.settings[UO_pos_conditional].tp & TP_LEAD))
         || (  chunk_is_token(ent.pc, CT_BOOL)
            && (cpd.settings[UO_pos_bool].tp & TP_LEAD)))
      {
         pc = ent.pc;
      }
      else
      {
         pc = chunk_get_next(ent.pc);
      }
      LOG_FMT(LSPLIT, "%s(%d): at %s, col=%zu\n", __func__, __LINE__, pc->text(), pc->orig_col);
   }

   if (pc == nullptr)
   {
      pc = start;
      // Don't break before a close, comma, or colon
      if (  start->type == CT_PAREN_CLOSE
         || start->type == CT_PAREN_OPEN
         || start->type == CT_FPAREN_CLOSE
         || start->type == CT_FPAREN_OPEN
         || start->type == CT_SPAREN_CLOSE
         || start->type == CT_SPAREN_OPEN
         || start->type == CT_ANGLE_CLOSE
         || start->type == CT_BRACE_CLOSE
         || start->type == CT_COMMA
         || start->type == CT_SEMICOLON
         || start->type == CT_VSEMICOLON
         || start->len() == 0)
      {
         LOG_FMT(LSPLIT, " ** NO GO **\n");

         // TODO: Add in logic to handle 'hard' limits by backing up a token
         return(true);
      }
   }

   // add a newline before pc
   prev = chunk_get_prev(pc);
   if (  (prev != nullptr)
      && !chunk_is_newline(pc)
      && !chunk_is_newline(prev))
   {
      //int plen = (pc->len() < 5) ? pc->len() : 5;
      //int slen = (start->len() < 5) ? start->len() : 5;
      //LOG_FMT(LSPLIT, " '%.*s' [%s], started on token '%.*s' [%s]\n",
      //        plen, pc->text(), get_token_name(pc->type),
      //        slen, start->text(), get_token_name(start->type));
      LOG_FMT(LSPLIT, "  %s(%d): %s [%s], started on token '%s' [%s]\n",
              __func__, __LINE__, pc->text(), get_token_name(pc->type),
              start->text(), get_token_name(start->type));

      split_before_chunk(pc);
   }
   return(true);
} // split_line


/*
 * The for statement split algorithm works as follows:
 *   1. Step backwards and forwards to find the semicolons
 *   2. Try splitting at the semicolons first.
 *   3. If that doesn't work, then look for a comma at paren level.
 *   4. If that doesn't work, then look for an assignment at paren level.
 *   5. If that doesn't work, then give up.
 */
static void split_for_stmt(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   // how many semicolons (1 or 2) do we need to find
   size_t  max_cnt     = cpd.settings[UO_ls_for_split_full].b ? 2 : 1;
   chunk_t *open_paren = nullptr;
   size_t  nl_cnt      = 0;

   LOG_FMT(LSPLIT, "%s: starting on %s, line %zu\n",
           __func__, start->text(), start->orig_line);

   // Find the open paren so we know the level and count newlines
   chunk_t *pc = start;
   while ((pc = chunk_get_prev(pc)) != nullptr)
   {
      if (pc->type == CT_SPAREN_OPEN)
      {
         open_paren = pc;
         break;
      }
      if (pc->nl_count > 0)
      {
         nl_cnt += pc->nl_count;
      }
   }
   if (open_paren == nullptr)
   {
      LOG_FMT(LSPLIT, "No open paren\n");
      return;
   }

   // see if we started on the semicolon
   int     count = 0;
   chunk_t *st[2];
   pc = start;
   if ((pc->type == CT_SEMICOLON) && (pc->parent_type == CT_FOR))
   {
      st[count++] = pc;
   }

   // first scan backwards for the semicolons
   while (  (count < static_cast<int>(max_cnt))
         && ((pc = chunk_get_prev(pc)) != nullptr)
         && (pc->flags & PCF_IN_SPAREN))
   {
      if ((pc->type == CT_SEMICOLON) && (pc->parent_type == CT_FOR))
      {
         st[count++] = pc;
      }
   }

   // And now scan forward
   pc = start;
   while (  (count < static_cast<int>(max_cnt))
         && ((pc = chunk_get_next(pc)) != nullptr)
         && (pc->flags & PCF_IN_SPAREN))
   {
      if ((pc->type == CT_SEMICOLON) && (pc->parent_type == CT_FOR))
      {
         st[count++] = pc;
      }
   }

   while (--count >= 0)
   {
      // TODO: st[0] may be uninitialized here
      LOG_FMT(LSPLIT, "%s: split before %s\n", __func__, st[count]->text());
      split_before_chunk(chunk_get_next(st[count]));
   }

   if (!is_past_width(start) || (nl_cnt > 0))
   {
      return;
   }

   // Still past width, check for commas at parenthese level
   pc = open_paren;
   while ((pc = chunk_get_next(pc)) != start)
   {
      if ((pc->type == CT_COMMA) && (pc->level == (open_paren->level + 1)))
      {
         split_before_chunk(chunk_get_next(pc));
         if (!is_past_width(pc))
         {
            return;
         }
      }
   }

   // Still past width, check for a assignments at parenthese level
   pc = open_paren;
   while ((pc = chunk_get_next(pc)) != start)
   {
      if ((pc->type == CT_ASSIGN) && (pc->level == (open_paren->level + 1)))
      {
         split_before_chunk(chunk_get_next(pc));
         if (!is_past_width(pc))
         {
            return;
         }
      }
   }
   // Oh, well. We tried.
} // split_for_stmt


static void split_fcn_params_full(chunk_t *start)
{
   LOG_FUNC_ENTRY();
   LOG_FMT(LSPLIT, "%s", __func__);
#ifdef DEBUG
   LOG_FMT(LSPLIT, "\n");
#endif // DEBUG

   // Find the opening function parenthesis
   chunk_t *fpo = start;
   while ((fpo = chunk_get_prev(fpo)) != nullptr)
   {
      LOG_FMT(LSPLIT, "%s: %s, orig_col=%zu, Level=%zu\n",
              __func__, fpo->text(), fpo->orig_col, fpo->level);
      if ((fpo->type == CT_FPAREN_OPEN) && (fpo->level == start->level - 1))
      {
         break;  // opening parenthesis found. Issue #1020
      }
   }

   // Now break after every comma
   chunk_t *pc = fpo;
   while ((pc = chunk_get_next_ncnl(pc)) != nullptr)
   {
      if (pc->level <= fpo->level)
      {
         break;
      }
      if ((pc->level == (fpo->level + 1)) && (pc->type == CT_COMMA))
      {
         split_before_chunk(chunk_get_next(pc));
      }
   }
}


//! adds a newline before the c chunk and re-indents it afterward with indent_col
static void newline_and_indent(chunk_t &c, uint_fast32_t indent_col)
{
   newline_add_before(&c);
   reindent_line(&c, indent_col);
   cpd.changes++;
}


static size_t check_func_expr(chunk_t &delim_start, chunk_t &delim_end, size_t indent_col)
{
   assert(&delim_start != &delim_end);
   assert(chunk_get_next(&delim_start) != &delim_end);

   std::cout << "\n"
             << "    check_func_expr:\n"
             << "        start: " << delim_start.column << " - " << delim_start.str.c_str() << "\n"
             << "          end: " << delim_end.column << " - " << delim_end.str.c_str() << "\n"
             << "----------------------------------------------" << std::endl;

   const int maximal_col_pos = static_cast<int>(cpd.settings[UO_code_width].u);
   int       last_char_pos   = delim_end.column + delim_end.len() - 1;

   // check for additional ';' after ')'
   if (delim_end.type == CT_FPAREN_CLOSE)
   {
      auto *after_delim_end = chunk_get_next_nc(&delim_end);
      if (after_delim_end != nullptr && after_delim_end->type == CT_SEMICOLON)
      {
         last_char_pos = after_delim_end->column + after_delim_end->len() - 1;
      }
   }

   // return if column positions are OK
   if (last_char_pos <= maximal_col_pos)
   {
      return(indent_col);
   }

   const auto indent_col_step = cpd.settings[UO_indent_columns].u;
   const auto continue_col    = cpd.settings[UO_indent_continue].u;


   auto first = chunk_get_next(&delim_start);
   assert(first->level != 0);
   if (continue_col != 0)
   {
      indent_col = (first->level - 1) * indent_col_step
                   + (continue_col == 0 ? indent_col_step : continue_col);
   }

   // check param expr length to see if it would fit on its own line
   //     f(int long_param_nameeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee);
   // vs
   //     long_class_nameeeeeeeeee::long_member_func_nameeeeeeeeeeeeeeeeeee(int a, int b);
   const auto line_first_statement = (  delim_start.type == CT_NEWLINE
                                     || delim_start.type == CT_FPAREN_OPEN);
   if (line_first_statement)
   {
      auto last = chunk_get_prev(&delim_end);
      assert(first != nullptr || last != nullptr);

      const int expr_len    = last_char_pos + 1 - first->column; // including ',' or ");"
      const int fitting_len = maximal_col_pos
                              - ((first->level - 1) * indent_col_step
                                 + (continue_col == 0
                                    ? indent_col_step : continue_col));

      // insert newline after delim_end if
      if (expr_len > fitting_len)                 // expression would not fit on the next line either
      {                                           // AND
         if (  !chunk_is_last_on_line(delim_end)  // delim_end is not followed by an newline already
            && delim_end.type != CT_FPAREN_CLOSE) // delim_end is not a closing parenthesis: '...);'
         {
            auto *after_end_delim = chunk_get_next(&delim_end);
            assert(after_end_delim != nullptr);

            newline_and_indent(*after_end_delim, indent_col);
         }
         // else: do nothing
      }
      // fits on a new line, recalculate indent_col
      else
      {
         indent_col = (first->level - 1) * indent_col_step
                      + (continue_col == 0 ? indent_col_step : continue_col);
         newline_and_indent(*first, indent_col);
      }
   }
   // is too long and not the first expr, so this will be moved to a new line
   else
   {
      newline_and_indent(*first, indent_col);
   }

   return(indent_col);
} // check_func_expr


static void split_fcn_params(chunk_t * &start)
{
   assert(start != nullptr);
   chunk_t *fpo = nullptr;
   chunk_t *fpc = nullptr;

   if (start->type == CT_FPAREN_OPEN)
   {
      fpo = start;
      fpc = chunk_skip_to_match(fpo);
   }
   else if (start->type == CT_FPAREN_CLOSE)
   {
      fpc = start;
      fpo = chunk_skip_to_match_rev(fpc);
   }
   else if (start->type == CT_SEMICOLON)
   {
      fpc = chunk_get_prev_type(start, CT_FPAREN_CLOSE, start->level);
      fpo = chunk_skip_to_match_rev(fpc);
   }
   else
   {
      fpo = chunk_get_prev_type(start, CT_FPAREN_OPEN, start->level - 1);
      fpc = chunk_skip_to_match(fpo);
   }

   if (fpc == nullptr || fpo == nullptr)
   {
      return;
   }

   // f()
   if (chunk_get_next(fpo) == fpc)
   {
      start = chunk_get_next(fpc);
      return;
   }


   chunk_t *nested     = nullptr;
   chunk_t *prev_delim = fpo;
   auto    *pc         = chunk_get_next(fpo);
   auto    min_pos     = pc->column;

   for ( ; pc != nullptr; pc = chunk_get_next(pc))
   {
      assert(pc->type != CT_SEMICOLON);
      std::cout << "    " << pc->str.c_str() << std::endl;

      switch (pc->type)
      {
      case CT_FPAREN_CLOSE:
      {
         // consider only fpc as delimiter
         if (pc != fpc)
         {
            break;
         }
         // intentional fallthrough on fpc
//         [[fallthrough]];  // TODO: use with c++17
      }

      case CT_COMMA:
      case CT_NEWLINE:
      {
         if (pc->type == CT_NEWLINE && chunk_get_prev(pc) == prev_delim)
         {
            prev_delim = pc; // re-assign prev_delim to prevent empty expressions: newline , or ( newline
         }
         else
         {
            min_pos = check_func_expr(*prev_delim, *pc, min_pos);

            // re-assign prev_delim, n: delim_end -> n+1: delim_start
            prev_delim = pc;

            if (nested != nullptr)
            {
               split_fcn_params(nested);
               nested = nullptr;
            }
         }
         break;
      }

      case CT_FPAREN_OPEN:
      {
         auto *next = chunk_get_next_ncnl(pc);
         assert(next != nullptr);

         if (next->type != CT_FPAREN_CLOSE) // ignore '()'
         {
            nested = next;
            pc     = chunk_skip_to_match(pc);
            assert(pc != nullptr);
         }
         break;
      }

      default:
      {
         break;
      }
      } // switch

      if (pc == fpc)
      {
         break;
      }
   }

   start = fpc;

   // prevent loop: ';' -> ')' -> ';'
   chunk_t *next = chunk_get_next(fpc);
   if (next != nullptr && next->type == CT_SEMICOLON)
   {
      start = next;
   }
} // split_fcn_params
