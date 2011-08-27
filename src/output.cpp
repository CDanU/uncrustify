/**
 * @file output.cpp
 * Does all the output & comment formatting.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */
#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "unc_ctype.h"
#include <cstring>
#include <cstdlib>

static void output_comment_multi(chunk_t *pc);
static void output_comment_multi_simple(chunk_t *pc);

struct cmt_reflow
{
   chunk_t    *pc;
   int        column;      /* Column of the comment start */
   int        brace_col;   /* Brace column (for indenting with tabs) */
   int        base_col;    /* Base column (for indenting with tabs) */
   int        word_count;  /* number of words on this line */
   bool       kw_subst;    /* do keyword substitution */
   int        xtra_indent; /* extra indent of non-first lines (0 or 1) */
   const char *cont_text;  /* fixed text to output at the start of a line (0 to 3 chars) */
   bool       reflow;      /* reflow the current line */
};


static chunk_t *output_comment_c(chunk_t *pc);
static chunk_t *output_comment_cpp(chunk_t *pc);
static void add_comment_text(const char *text, int len,
                             cmt_reflow& cmt, bool esc_close);

/**
 * All output text is sent here, one char at a time.
 */
static void add_char(UINT32 ch)
{
   static int last_char = 0;

   /* If we did a '\r' and it isn't followed by a '\n', then output a newline */
   if ((last_char == '\r') && (ch != '\n'))
   {
      write_string(cpd.fout, cpd.newline.get(), cpd.enc);
      cpd.column      = 1;
      cpd.did_newline = 1;
      cpd.spaces      = 0;
   }

   /* convert a newline into the LF/CRLF/CR sequence */
   if (ch == '\n')
   {
      write_string(cpd.fout, cpd.newline.get(), cpd.enc);
      cpd.column      = 1;
      cpd.did_newline = 1;
      cpd.spaces      = 0;
   }
   else if (ch == '\r')
   {
      /* do not output '\r' */
      cpd.column      = 1;
      cpd.did_newline = 1;
      cpd.spaces      = 0;
   }
   else
   {
      /* Explicitly disallow a tab after a space */
      if ((ch == '\t') && (last_char == ' '))
      {
         int endcol = next_tab_column(cpd.column);
         while (cpd.column < endcol)
         {
            add_char(' ');
         }
         return;
      }
      else if (ch == ' ')
      {
         cpd.spaces++;
         cpd.column++;
      }
      else
      {
         while (cpd.spaces > 0)
         {
            write_char(cpd.fout, ' ', cpd.enc);
            cpd.spaces--;
         }
         write_char(cpd.fout, ch, cpd.enc);
         if (ch == '\t')
         {
            cpd.column = next_tab_column(cpd.column);
         }
         else
         {
            cpd.column++;
         }
      }
   }
   last_char = ch;
}


static void add_text(const char *text)
{
   char ch;

   while ((ch = *text) != 0)
   {
      text++;
      add_char(ch);
   }
}


static void add_text_len(const char *text, int len)
{
   while (len-- > 0)
   {
      add_char(*text);
      text++;
   }
}


static void add_text(const unc_text& text)
{
   for (int idx = 0; idx < text.size(); idx++)
   {
      add_char(text[idx]);
   }
}


/**
 * Count the number of characters to the end of the next chunk of text.
 * If it exceeds the limit, return true.
 */
static bool next_word_exceeds_limit(const char *text)
{
   int length = 0;

   /* Count any whitespace */
   while ((*text != 0) && unc_isspace(*text))
   {
      text++;
      length++;
   }

   /* Count non-whitespace */
   while ((*text != 0) && !unc_isspace(*text))
   {
      text++;
      length++;
   }
   return((cpd.column + length - 1) > cpd.settings[UO_cmt_width].n);
}


/**
 * Advance to a specific column
 * cpd.column is the current column
 *
 * @param column  The column to advance to
 */
static void output_to_column(int column, bool allow_tabs)
{
   int nc;

   cpd.did_newline = 0;
   if (allow_tabs)
   {
      /* tab out as far as possible and then use spaces */
      while ((nc = next_tab_column(cpd.column)) <= column)
      {
         add_text("\t");
      }
   }
   /* space out the final bit */
   while (cpd.column < column)
   {
      add_text(" ");
   }
}


/**
 * Output a comment to the column using indent_with_tabs and
 * indent_cmt_with_tabs as the rules.
 * base_col is the indent of the first line of the comment.
 * On the first line, column == base_col.
 * On subsequnet lines, column >= base_col.
 *
 * @param brace_col the brace-level indent of the comment
 * @param base_col  the indent of the start of the comment (multiline)
 * @param column    the column that we should end up in
 */
static void cmt_output_indent(int brace_col, int base_col, int column)
{
   int iwt;
   int tab_col;

   iwt = cpd.settings[UO_indent_cmt_with_tabs].b ? 2 :
         (cpd.settings[UO_indent_with_tabs].n ? 1 : 0);

   tab_col = (iwt == 0) ? 0 : ((iwt == 1) ? brace_col : base_col);

   //LOG_FMT(LSYS, "%s(brace=%d base=%d col=%d iwt=%d) tab=%d cur=%d\n",
   //        __func__, brace_col, base_col, column, iwt, tab_col, cpd.column);

   cpd.did_newline = 0;
   if ((iwt == 2) || ((cpd.column == 1) && (iwt == 1)))
   {
      /* tab out as far as possible and then use spaces */
      while (next_tab_column(cpd.column) <= tab_col)
      {
         add_text("\t");
      }
   }

   /* space out the rest */
   while (cpd.column < column)
   {
      add_text(" ");
   }
}


void output_parsed(FILE *pfile)
{
   chunk_t *pc;
   int     cnt;

   output_options(pfile);
   output_defines(pfile);
   output_types(pfile);

   fprintf(pfile, "-=====-\n");
   fprintf(pfile, "Line      Tag          Parent     Columns  Br/Lvl/pp Flag Nl  Text");
   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      fprintf(pfile, "\n%3d> %13.13s[%13.13s][%2d/%2d/%2d][%d/%d/%d][%10" PRIx64 "][%d-%d]",
              pc->orig_line, get_token_name(pc->type),
              get_token_name(pc->parent_type),
              pc->column, pc->orig_col, pc->orig_col_end,
              pc->brace_level, pc->level, pc->pp_level,
              pc->flags, pc->nl_count, pc->after_tab);

      if ((pc->type != CT_NEWLINE) && (pc->len() != 0))
      {
         for (cnt = 0; cnt < pc->column; cnt++)
         {
            fprintf(pfile, " ");
         }
         if (pc->type != CT_NL_CONT)
         {
            fprintf(pfile, "%s", pc->str.c_str());
         }
         else
         {
            fprintf(pfile, "\\");
         }
      }
   }
   fprintf(pfile, "\n-=====-\n");
   fflush(pfile);
}


void output_options(FILE *pfile)
{
   int idx;
   const option_map_value *ptr;

   fprintf(pfile, "-== Options ==-\n");
   for (idx = 0; idx < UO_option_count; idx++)
   {
      ptr = get_option_name(idx);
      if (ptr != NULL)
      {
         if (ptr->type == AT_STRING)
         {
            fprintf(pfile, "%3d) %32s = \"%s\"\n",
                    ptr->id, ptr->name,
                    op_val_to_string(ptr->type, cpd.settings[ptr->id]).c_str());
         }
         else
         {
            fprintf(pfile, "%3d) %32s = %s\n",
                    ptr->id, ptr->name,
                    op_val_to_string(ptr->type, cpd.settings[ptr->id]).c_str());
         }
      }
   }
}


/**
 * This renders the chunk list to a file.
 */
void output_text(FILE *pfile)
{
   chunk_t *pc;
   chunk_t *prev;
   int     cnt;
   int     lvlcol;
   bool    allow_tabs;

   cpd.fout = pfile;

   cpd.did_newline = 1;
   cpd.column      = 1;

   if (cpd.bom != NULL)
   {
      add_text(cpd.bom->str);
      cpd.did_newline = 1;
      cpd.column      = 1;
   }

   if (cpd.frag_cols > 0)
   {
      int indent = cpd.frag_cols - 1;

      for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
      {
         pc->column        += indent;
         pc->column_indent += indent;
      }
      cpd.frag_cols = 0;
   }

   for (pc = chunk_get_head(); pc != NULL; pc = chunk_get_next(pc))
   {
      if (pc->type == CT_NEWLINE)
      {
         for (cnt = 0; cnt < pc->nl_count; cnt++)
         {
            add_char('\n');
         }
         cpd.did_newline = 1;
         cpd.column      = 1;
         LOG_FMT(LOUTIND, " xx\n");
      }
      else if (pc->type == CT_NL_CONT)
      {
         /* FIXME: this really shouldn't be done here! */
         if ((pc->flags & PCF_WAS_ALIGNED) == 0)
         {
            if (cpd.settings[UO_sp_before_nl_cont].a & AV_REMOVE)
            {
               pc->column = cpd.column + (cpd.settings[UO_sp_before_nl_cont].a == AV_FORCE);
            }
            else
            {
               /* Try to keep the same relative spacing */
               prev = chunk_get_prev(pc);
               while ((prev != NULL) && (prev->orig_col == 0) && (prev->nl_count == 0))
               {
                  prev = chunk_get_prev(prev);
               }

               if ((prev != NULL) && (prev->nl_count == 0))
               {
                  int orig_sp = (pc->orig_col - prev->orig_col_end);
                  pc->column = cpd.column + orig_sp;
                  if ((cpd.settings[UO_sp_before_nl_cont].a != AV_IGNORE) &&
                      (pc->column < (cpd.column + 1)))
                  {
                     pc->column = cpd.column + 1;
                  }
               }
            }
         }
         output_to_column(pc->column, (cpd.settings[UO_indent_with_tabs].n == 2));
         add_char('\\');
         add_char('\n');
         cpd.did_newline = 1;
         cpd.column      = 1;
         LOG_FMT(LOUTIND, " \\xx\n");
      }
      else if (pc->type == CT_COMMENT_MULTI)
      {
         if (cpd.settings[UO_cmt_indent_multi].b)
         {
            output_comment_multi(pc);
         }
         else
         {
            output_comment_multi_simple(pc);
         }
      }
      else if (pc->type == CT_COMMENT_CPP)
      {
         pc = output_comment_cpp(pc);
      }
      else if (pc->type == CT_COMMENT)
      {
         pc = output_comment_c(pc);
      }
      else if ((pc->type == CT_JUNK) || (pc->type == CT_IGNORED))
      {
         /* do not adjust the column for junk */
         add_text(pc->str);
      }
      else if (pc->len() == 0)
      {
         /* don't do anything for non-visible stuff */
         LOG_FMT(LOUTIND, " <%d> -", pc->column);
      }
      else
      {
         /* indent to the 'level' first */
         if (cpd.did_newline)
         {
            if (cpd.settings[UO_indent_with_tabs].n == 1)
            {
               /* FIXME: it would be better to properly set column_indent in
                * indent_text(), but this hack for '}' and ':' seems to work. */
               if ((pc->type == CT_BRACE_CLOSE) ||
                   chunk_is_str(pc, ":", 1) ||
                   (pc->type == CT_PREPROC))
               {
                  lvlcol = pc->column;
               }
               else
               {
                  lvlcol = pc->column_indent;
                  if (lvlcol > pc->column)
                  {
                     lvlcol = pc->column;
                  }
               }

               if (lvlcol > 1)
               {
                  output_to_column(lvlcol, true);
               }
            }
            allow_tabs = (cpd.settings[UO_indent_with_tabs].n == 2) ||
                         (chunk_is_comment(pc) &&
                          (cpd.settings[UO_indent_with_tabs].n != 0));

            LOG_FMT(LOUTIND, "  %d> col %d/%d - ", pc->orig_line, pc->column, cpd.column);
         }
         else
         {
            /**
             * Reformatting multi-line comments can screw up the column.
             * Make sure we don't mess up the spacing on this line.
             * This has to be done here because comments are not formatted
             * until the output phase.
             */
            if (pc->column < cpd.column)
            {
               reindent_line(pc, cpd.column);
            }

            /* not the first item on a line */
            prev       = chunk_get_prev(pc);
            allow_tabs = (cpd.settings[UO_align_with_tabs].b &&
                          ((pc->flags & PCF_WAS_ALIGNED) != 0) &&
                          ((prev->column + prev->len() + 1) != pc->column));
            if (cpd.settings[UO_align_keep_tabs].b)
            {
               allow_tabs |= pc->after_tab;
            }
            LOG_FMT(LOUTIND, " %d(%d) -", pc->column, allow_tabs);
         }

         output_to_column(pc->column, allow_tabs);
         add_text(pc->str);
         cpd.did_newline = chunk_is_newline(pc);
      }
   }
}

/**
 * Checks for and updates the lead chars.
 *
 * @param line the comment line
 * @return 0=not present, >0=number of chars that are part of the lead
 */
static int cmt_parse_lead(const char *line, int is_last)
{
   int len = 0;

   while ((len < 32) && (line[len] != 0))
   {
      if ((len > 0) && (line[len] == '/'))
      {
         /* ignore combined comments */
         int tmp = len + 1;
         while (unc_isspace(line[tmp]))
         {
            tmp++;
         }
         if (line[tmp] == '/')
         {
            return 1;
         }
         break;
      }
      else if (strchr("*|\\#+", line[len]) == NULL)
      {
         break;
      }
      len++;
   }

   if (len > 30)
   {
      return 1;
   }

   if ((len > 0) && ((line[len] == 0) || unc_isspace(line[len])))
   {
      return len;
   }
   if (is_last && (len > 0))
   {
      return len;
   }
   return 0;
}

/**
 * Scans a multiline comment to determine the following:
 *  - the extra indent of the non-first line (0 or 1)
 *  - the continuation text ('' or '* ')
 *
 * The decision is based on:
 *  - cmt_indent_multi
 *  - cmt_star_cont
 *  - the first line length
 *  - the second line leader length
 *  - the last line length
 *
 * If the first and last line are the same length and don't contain any alnum
 * chars and (the first line len > 2 or the second leader is the same as the
 * first line length), then the indent is 0.
 *
 * If the leader on the second line is 1 wide or missing, then the indent is 1.
 *
 * Otherwise, the indent is 0.
 *
 * @param str       The comment string
 * @param len       Length of the comment
 * @param start_col Starting column
 * @return 0 or 1
 */
static void calculate_comment_body_indent(cmt_reflow &cmt, const char *str, int len)
{
   int idx       = 0;
   int first_len = 0;
   int last_len  = 0;
   int width     = 0;

   cmt.xtra_indent = 0;

   if (!cpd.settings[UO_cmt_indent_multi].b)
   {
      return;
   }

   if (cpd.settings[UO_cmt_multi_check_last].b)
   {
      /* find the last line length */
      for (idx = len - 1; idx > 0; idx--)
      {
         if ((str[idx] == '\n') || (str[idx] == '\r'))
         {
            idx++;
            while ((idx < len) && ((str[idx] == ' ') || (str[idx] == '\t')))
            {
               idx++;
            }
            last_len = len - idx;
            break;
         }
      }
   }

   /* find the first line length */
   for (idx = 0; idx < len; idx++)
   {
      if ((str[idx] == '\n') || (str[idx] == '\r'))
      {
         first_len = idx;
         while ((str[first_len - 1] == ' ') || (str[first_len - 1] == '\t'))
         {
            first_len--;
         }

         /* handle DOS endings */
         if ((str[idx] == '\r') && (str[idx + 1] == '\n'))
         {
            idx++;
         }
         idx++;
         break;
      }
   }

   /* Scan the second line */
   width = 0;
   for (/* nada */; idx < len - 1; idx++)
   {
      if ((str[idx] == ' ') || (str[idx] == '\t'))
      {
         if (width > 0)
         {
            break;
         }
         continue;
      }
      if ((str[idx] == '\n') || (str[idx] == '\r'))
      {
         /* Done with second line */
         break;
      }

      /* Count the leading chars */
      if ((str[idx] == '*') ||
          (str[idx] == '|') ||
          (str[idx] == '\\') ||
          (str[idx] == '#') ||
          (str[idx] == '+'))
      {
         width++;
      }
      else
      {
         width = 0;
         break;
      }
   }

   //LOG_FMT(LSYS, "%s: first=%d last=%d width=%d\n", __func__, first_len, last_len, width);

   /*TODO: make the first_len minimum (4) configurable? */
   if ((first_len == last_len) && ((first_len > 4) || (first_len == width)))
   {
      return;
   }

   cmt.xtra_indent = ((width == 2) ? 0 : 1);
}


static chunk_t *get_next_function(chunk_t *pc)
{
   while ((pc = chunk_get_next(pc)) != NULL)
   {
      if ((pc->type == CT_FUNC_DEF) ||
          (pc->type == CT_OC_MSG_DECL) ||
          (pc->type == CT_FUNC_PROTO))
      {
         return(pc);
      }
   }
   return(NULL);
}


static chunk_t *get_next_class(chunk_t *pc)
{
   while ((pc = chunk_get_next(pc)) != NULL)
   {
      if (pc->type == CT_CLASS)
      {
         return(chunk_get_next(pc));
      }
   }
   return(NULL);
}


/**
 * Adds the javadoc-style @param and @return stuff, based on the params and
 * return value for pc.
 * If the arg list is '()' or '(void)', then no @params are added.
 * Likewise, if the return value is 'void', then no @return is added.
 */
static void add_comment_javaparam(chunk_t *pc, cmt_reflow& cmt)
{
   chunk_t *fpo;
   chunk_t *fpc;
   chunk_t *tmp;
   chunk_t *prev;
   bool    has_param = true;
   bool    need_nl   = false;

   fpo = chunk_get_next_type(pc, CT_FPAREN_OPEN, pc->level);
   if (fpo == NULL)
   {
      return;
   }
   fpc = chunk_get_next_type(fpo, CT_FPAREN_CLOSE, pc->level);
   if (fpc == NULL)
   {
      return;
   }

   /* Check for 'foo()' and 'foo(void)' */
   if (chunk_get_next_ncnl(fpo) == fpc)
   {
      has_param = false;
   }
   else
   {
      tmp = chunk_get_next_ncnl(fpo);
      if ((tmp == chunk_get_prev_ncnl(fpc)) &&
          chunk_is_str(tmp, "void", 4))
      {
         has_param = false;
      }
   }

   if (has_param)
   {
      tmp  = fpo;
      prev = NULL;
      while ((tmp = chunk_get_next(tmp)) != NULL)
      {
         if ((tmp->type == CT_COMMA) || (tmp == fpc))
         {
            if (need_nl)
            {
               add_comment_text("\n ", 2, cmt, false);
            }
            need_nl = true;
            add_text("@param");
            if (prev != NULL)
            {
               add_text(" ");
               add_text(prev->str);
               add_text(" TODO");
            }
            prev = NULL;
            if (tmp == fpc)
            {
               break;
            }
         }
         if (tmp->type == CT_WORD)
         {
            prev = tmp;
         }
      }
   }

   /* Do the return stuff */
   tmp = chunk_get_prev_ncnl(pc);
   if ((tmp != NULL) && !chunk_is_str(tmp, "void", 4))
   {
      if (need_nl)
      {
         add_comment_text("\n ", 2, cmt, false);
      }
      add_text("@return TODO");
   }
}


/**
 * text starts with '$('. see if this matches a keyword and add text based
 * on that keyword.
 * @return the number of characters eaten from the text
 */
static int add_comment_kw(const char *text, int len, cmt_reflow& cmt)
{
   if ((len >= 11) && (memcmp(text, "$(filename)", 11) == 0))
   {
      add_text(path_basename(cpd.filename));
      return(11);
   }
   if ((len >= 8) && (memcmp(text, "$(class)", 8) == 0))
   {
      chunk_t *tmp = get_next_class(cmt.pc);
      if (tmp != NULL)
      {
         add_text(tmp->str);
         return(8);
      }
   }

   /* If we can't find the function, we are done */
   chunk_t *fcn = get_next_function(cmt.pc);
   if (fcn == NULL)
   {
      return(0);
   }

   if ((len >= 10) && (memcmp(text, "$(message)", 10) == 0))
   {
      add_text(fcn->str);
      chunk_t *tmp = chunk_get_next_ncnl(fcn);
      chunk_t *word = NULL;
      while (tmp)
      {
         if ((tmp->type == CT_BRACE_OPEN) || (tmp->type == CT_SEMICOLON))
         {
            break;
         }
         if (tmp->type == CT_OC_COLON)
         {
            if (word != NULL)
            {
               add_text(word->str);
               word = NULL;
            }
            add_text(":");
         }
         if (tmp->type == CT_WORD)
         {
            word = tmp;
         }
         tmp = chunk_get_next_ncnl(tmp);
      }
      return(10);
   }
   if ((len >= 11) && (memcmp(text, "$(function)", 11) == 0))
   {
      if (fcn->parent_type == CT_OPERATOR)
      {
         add_text("operator ");
      }
      add_text(fcn->str);
      return(11);
   }
   if ((len >= 12) && (memcmp(text, "$(javaparam)", 12) == 0))
   {
      add_comment_javaparam(fcn, cmt);
      return(12);
   }
   if ((len >= 9) && (memcmp(text, "$(fclass)", 9) == 0))
   {
      chunk_t *tmp = chunk_get_prev_ncnl(fcn);
      if ((tmp != NULL) && (tmp->type == CT_OPERATOR))
      {
         tmp = chunk_get_prev_ncnl(tmp);
      }
      if ((tmp != NULL) && ((tmp->type == CT_DC_MEMBER) ||
                            (tmp->type == CT_MEMBER)))
      {
         tmp = chunk_get_prev_ncnl(tmp);
         add_text(tmp->str);
         return(9);
      }
   }
   return(0);
}


static int next_up(const char *text, int text_len, const char *tag)
{
   int offs    = 0;
   int tag_len = strlen(tag);

   while (unc_isspace(*text) && (text_len > 0))
   {
      text++;
      text_len--;
      offs++;
   }

   if ((tag_len <= text_len) && (memcmp(text, tag, tag_len) == 0))
   {
      return(offs);
   }
   return(-1);
}


/**
 * Outputs a comment. The initial opening '//' may be included in the text.
 * Subsequent openings (if combining comments), should not be included.
 * The closing (for C/D comments) should not be included.
 *
 * TODO:
 * If reflowing text, the comment should be added one word (or line) at a time.
 * A newline should only be sent if a blank line is encountered or if the next
 * line is indented beyond the current line (optional?).
 * If the last char on a line is a ':' or '.', then the next line won't be
 * combined.
 */
static void add_comment_text(const char *text, int len,
                             cmt_reflow& cmt, bool esc_close)
{
   bool was_star   = false;
   bool was_slash  = false;
   bool was_dollar = false;
   bool in_word    = false;
   int  tmp;

   for (int idx = 0; idx < len; idx++)
   {
      if (!was_dollar && cmt.kw_subst &&
          (text[idx] == '$') && (len > (idx + 3)) && (text[idx + 1] == '('))
      {
         idx += add_comment_kw(&text[idx], len - idx, cmt);
         if (idx >= len)
         {
            break;
         }
      }

      /* Split the comment */
      if (text[idx] == '\n')
      {
         in_word = false;
         add_char('\n');
         cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
         if (cmt.xtra_indent)
         {
            add_char(' ');
         }

         /* hack to get escaped newlines to align and not dup the leading '//' */
         tmp = next_up(text + idx + 1, len - (idx + 1), cmt.cont_text);
         if (tmp < 0)
         {
            add_text(cmt.cont_text);
         }
         else
         {
            idx += tmp;
         }
      }
      else if (cmt.reflow &&
               (text[idx] == ' ') &&
               (cpd.settings[UO_cmt_width].n > 0) &&
               ((cpd.column > cpd.settings[UO_cmt_width].n) ||
                next_word_exceeds_limit(text + idx)))
      {
         in_word = false;
         add_char('\n');
         cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
         if (cmt.xtra_indent)
         {
            add_char(' ');
         }
         add_text(cmt.cont_text);
      }
      else
      {
         /* Escape a C closure in a CPP comment */
         if (esc_close &&
             ((was_star && (text[idx] == '/')) ||
              (was_slash && (text[idx] == '*'))))
         {
            add_char(' ');
         }
         if (!in_word && !unc_isspace(text[idx]))
         {
            cmt.word_count++;
         }
         in_word = !unc_isspace(text[idx]);
         add_char(text[idx]);
         was_star   = (text[idx] == '*');
         was_slash  = (text[idx] == '/');
         was_dollar = (text[idx] == '$');
      }
   }
}


static void output_cmt_start(cmt_reflow& cmt, chunk_t *pc)
{
   cmt.pc          = pc;
   cmt.column      = pc->column;
   cmt.brace_col   = pc->column_indent;
   cmt.base_col    = pc->column_indent;
   cmt.word_count  = 0;
   cmt.kw_subst    = false;
   cmt.xtra_indent = 0;
   cmt.cont_text   = "";
   cmt.reflow      = false;

   if (cmt.brace_col == 0)
   {
      cmt.brace_col = 1 + (pc->brace_level * cpd.settings[UO_output_tab_size].n);
   }

   //LOG_FMT(LSYS, "%s: line %d, brace=%d base=%d col=%d orig=%d aligned=%x\n",
   //        __func__, pc->orig_line, cmt.brace_col, cmt.base_col, cmt.column, pc->orig_col,
   //        pc->flags & (PCF_WAS_ALIGNED | PCF_RIGHT_COMMENT));

   if ((pc->parent_type == CT_COMMENT_START) ||
       (pc->parent_type == CT_COMMENT_WHOLE))
   {
      if (!cpd.settings[UO_indent_col1_comment].b &&
          (pc->orig_col == 1) &&
          !(pc->flags & PCF_INSERTED))
      {
         cmt.column    = 1;
         cmt.base_col  = 1;
         cmt.brace_col = 1;
      }
   }
   else if (pc->parent_type == CT_COMMENT_END)
   {
      /* Make sure we have at least one space past the last token */
      chunk_t *prev = chunk_get_prev(pc);
      if (prev != NULL)
      {
         int col_min = prev->column + prev->len() + 1;
         if (cmt.column < col_min)
         {
            cmt.column = col_min;
         }
      }
   }

   /* tab aligning code */
   if (cpd.settings[UO_indent_cmt_with_tabs].b &&
       ((pc->parent_type == CT_COMMENT_END) ||
        (pc->parent_type == CT_COMMENT_WHOLE)))
   {
      cmt.column = align_tab_column(cmt.column - 1);
      //LOG_FMT(LSYS, "%s: line %d, orig:%d new:%d\n",
      //        __func__, pc->orig_line, pc->column, cmt.column);
      pc->column = cmt.column;
   }
   cmt.base_col = cmt.column;

   //LOG_FMT(LSYS, "%s: -- brace=%d base=%d col=%d\n",
   //        __func__, cmt.brace_col, cmt.base_col, cmt.column);

   /* Bump out to the column */
   cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);

   cmt.kw_subst = (pc->flags & PCF_INSERTED) != 0;
}


/**
 * Checks to see if the current comment can be combined with the next comment.
 * The two can be combined if:
 *  1. They are the same type
 *  2. There is exactly one newline between then
 *  3. They are indented to the same level
 */
static bool can_combine_comment(chunk_t *pc, cmt_reflow& cmt)
{
   /* We can't combine if there is something other than a newline next */
   if (pc->parent_type == CT_COMMENT_START)
   {
      return(false);
   }

   /* next is a newline for sure, make sure it is a single newline */
   chunk_t *next = chunk_get_next(pc);
   if ((next != NULL) && (next->nl_count == 1))
   {
      /* Make sure the comment is the same type at the same column */
      next = chunk_get_next(next);
      if ((next != NULL) &&
          (next->type == pc->type) &&
          (((next->column == 1) && (pc->column == 1)) ||
           ((next->column == cmt.base_col) && (pc->column == cmt.base_col)) ||
           ((next->column > cmt.base_col) && (pc->parent_type == CT_COMMENT_END))))
      {
         return(true);
      }
   }
   return(false);
}


/**
 * Outputs the C comment at pc.
 * C comment combining is done here
 *
 * @return the last chunk output'd
 */
static chunk_t *output_comment_c(chunk_t *first)
{
   cmt_reflow cmt;

   output_cmt_start(cmt, first);
   cmt.reflow = (cpd.settings[UO_cmt_reflow_mode].n != 1);

   cmt.cont_text = cpd.settings[UO_cmt_star_cont].b ? " *" : "  ";

   /* See if we can combine this comment with the next comment */
   if (!cpd.settings[UO_cmt_c_group].b ||
       !can_combine_comment(first, cmt))
   {
      /* Just add the single comment */
      add_comment_text(first->str, first->len(), cmt, false);
      return(first);
   }

   add_text("/*");
   if (cpd.settings[UO_cmt_c_nl_start].b)
   {
      add_comment_text("\n", 1, cmt, false);
   }
   chunk_t *pc = first;
   while (can_combine_comment(pc, cmt))
   {
      add_comment_text(pc->text() + 2, pc->len() - 4, cmt, false);
      add_comment_text("\n", 1, cmt, false);
      pc = chunk_get_next(chunk_get_next(pc));
   }
   add_comment_text(pc->text() + 2, pc->len() - 4, cmt, false);
   if (cpd.settings[UO_cmt_c_nl_end].b)
   {
      cmt.cont_text = " ";
      add_comment_text("\n", 1, cmt, false);
   }
   add_comment_text("*/", 2, cmt, false);
   return(pc);
}


/**
 * Outputs the CPP comment at pc.
 * CPP comment combining is done here
 *
 * @return the last chunk output'd
 */
static chunk_t *output_comment_cpp(chunk_t *first)
{
   cmt_reflow cmt;

   output_cmt_start(cmt, first);
   cmt.reflow = (cpd.settings[UO_cmt_reflow_mode].n != 1);

   /* CPP comments can't be grouped unless they are converted to C comments */
   if (!cpd.settings[UO_cmt_cpp_to_c].b)
   {
      cmt.cont_text = (cpd.settings[UO_sp_cmt_cpp_start].a & AV_REMOVE) ? "//" : "// ";

      if (cpd.settings[UO_sp_cmt_cpp_start].a == AV_IGNORE)
      {
         add_comment_text(first->text(), first->len(), cmt, false);
      }
      else
      {
         add_comment_text(first->str, 2, cmt, false);

         const char *tmp = first->text() + 2;
         int        len  = first->len() - 2;

         if (cpd.settings[UO_sp_cmt_cpp_start].a & AV_REMOVE)
         {
            while ((len > 0) && unc_isspace(*tmp))
            {
               len--;
               tmp++;
            }
         }
         if (len > 0)
         {
            if (cpd.settings[UO_sp_cmt_cpp_start].a & AV_ADD)
            {
               if (!unc_isspace(*tmp))
               {
                  add_comment_text(" ", 1, cmt, false);
               }
            }
            add_comment_text(tmp, len, cmt, false);
         }
      }

      return(first);
   }

   /* We are going to convert the CPP comments to C comments */
   cmt.cont_text = cpd.settings[UO_cmt_star_cont].b ? " * " : "   ";

   /* See if we can combine this comment with the next comment */
   if (!cpd.settings[UO_cmt_cpp_group].b ||
       !can_combine_comment(first, cmt))
   {
      /* nothing to group: just output a single line */
      add_text("/*");
      if (!unc_isspace(first->str[2]))
      {
         add_char(' ');
      }
      add_comment_text(first->text() + 2, first->len() - 2, cmt, true);
      add_text(" */");
      return(first);
   }

   add_text("/*");
   if (cpd.settings[UO_cmt_cpp_nl_start].b)
   {
      add_comment_text("\n", 1, cmt, false);
   }
   else
   {
      add_text(" ");
   }
   chunk_t *pc = first;

   int offs;
   while (can_combine_comment(pc, cmt))
   {
      offs = unc_isspace(pc->str[2]) ? 1 : 0;
      add_comment_text(pc->text() + 2 + offs, pc->len() - (2 + offs), cmt, true);
      add_comment_text("\n", 1, cmt, false);
      pc = chunk_get_next(chunk_get_next(pc));
   }
   offs = unc_isspace(pc->str[2]) ? 1 : 0;
   add_comment_text(pc->text() + 2 + offs, pc->len() - (2 + offs), cmt, true);
   if (cpd.settings[UO_cmt_cpp_nl_end].b)
   {
      cmt.cont_text = "";
      add_comment_text("\n", 1, cmt, false);
   }
   add_comment_text(" */", 3, cmt, false);
   return(pc);
}


static void cmt_trim_whitespace(int& line_len, char *line, bool in_preproc)
{
   /* Remove trailing whitespace on the line */
   while ((line_len > 0) &&
          ((line[line_len - 1] == ' ') ||
           (line[line_len - 1] == '\t')))
   {
      line_len--;
   }

   /* If in a preproc, shift any bs-nl back to the comment text */
   if (in_preproc && (line_len > 1) && (line[line_len - 1] == '\\'))
   {
      bool do_space = false;

      /* If there was any space before the backslash, change it to 1 space */
      line_len--;
      while ((line_len > 0) &&
             ((line[line_len - 1] == ' ') ||
              (line[line_len - 1] == '\t')))
      {
         do_space = true;
         line_len--;
      }
      if (do_space)
      {
         line[line_len++] = ' ';
      }
      line[line_len++] = '\\';
   }
}


/**
 * A multiline comment -- woopeee!
 * The only trick here is that we have to trim out whitespace characters
 * to get the comment to line up.
 */
static void output_comment_multi(chunk_t *pc)
{
   int        cmt_col;
   const char *cmt_str;
   int        remaining;
   char       ch;
   chunk_t    *prev;
   char       *line;
   int        line_len;
   int        line_count = 0;
   int        ccol; /* the col of subsequent comment lines */
   int        col_diff = 0;
   char       lead[80];
   bool       nl_end = false;
   cmt_reflow cmt;

   //LOG_FMT(LSYS, "%s: line %d\n", __func__, pc->orig_line);

   prev = chunk_get_prev(pc);
   output_cmt_start(cmt, pc);
   cmt.reflow = (cpd.settings[UO_cmt_reflow_mode].n != 1);

   cmt_col = cmt.base_col;
   col_diff = pc->orig_col - cmt.base_col;

   calculate_comment_body_indent(cmt, pc->text(), pc->len());

   cmt.cont_text = !cpd.settings[UO_cmt_indent_multi].b ? "" :
                   (cpd.settings[UO_cmt_star_cont].b ? "* " : "  ");

   //LOG_FMT(LSYS, "Indenting1 line %d to col %d (orig=%d) col_diff=%d xtra=%d cont='%s'\n",
   //        pc->orig_line, cmt_col, pc->orig_col, col_diff, cmt.xtra_indent, cmt.cont_text);

   ccol      = pc->column;
   remaining = pc->len();
   cmt_str   = pc->text();
   line_len  = 0;
   line      = new char[remaining + 1024 + 1]; /* + 1 for '\0' */
   while (remaining > 0)
   {
      ch = *cmt_str;
      cmt_str++;
      remaining--;

      /* handle the CRLF and CR endings. convert both to LF */
      if (ch == '\r')
      {
         ch = '\n';
         if (*cmt_str == '\n')
         {
            cmt_str++;
            remaining--;
         }
      }

      /* Find the start column */
      if (line_len == 0)
      {
         nl_end = false;
         if (ch == ' ')
         {
            ccol++;
            continue;
         }
         else if (ch == '\t')
         {
            ccol = calc_next_tab_column(ccol, cpd.settings[UO_input_tab_size].n);
            continue;
         }
         else
         {
            //LOG_FMT(LSYS, "%d] Text starts in col %d\n", line_count, ccol);
         }
      }

      /*
       * Now see if we need/must fold the next line with the current to enable
       * full reflow
       */
      if ((cpd.settings[UO_cmt_reflow_mode].n == 2) &&
          (ch == '\n') &&
          (remaining > 0))
      {
         int  nxt_len            = 0;
         int  next_nonempty_line = -1;
         int  prev_nonempty_line = -1;
         int  nwidx          = line_len;
         bool star_is_bullet = false;

         /* strip trailing whitespace from the line collected so far */
         line[nwidx] = 0; // sentinel
         while (nwidx > 0)
         {
            nwidx--;
            if ((prev_nonempty_line < 0) &&
                !unc_isspace(line[nwidx]) &&
                (line[nwidx] != '*') && // block comment: skip '*' at end of line
                ((pc->flags & PCF_IN_PREPROC)
                 ? (line[nwidx] != '\\') ||
                 ((line[nwidx + 1] != 'r') &&
                  (line[nwidx + 1] != '\n'))
                 : true))
            {
               prev_nonempty_line = nwidx; // last nonwhitespace char in the previous line
            }
         }

         for (nxt_len = 0;
              (nxt_len <= remaining) &&
              (cmt_str[nxt_len] != 'r') &&
              (cmt_str[nxt_len] != '\n');
              nxt_len++)
         {
            if ((next_nonempty_line < 0) &&
                !unc_isspace(cmt_str[nxt_len]) &&
                (cmt_str[nxt_len] != '*') &&
                ((nxt_len == remaining) ||
                 ((pc->flags & PCF_IN_PREPROC)
                  ? (cmt_str[nxt_len] != '\\') ||
                  ((cmt_str[nxt_len + 1] != 'r') &&
                   (cmt_str[nxt_len + 1] != '\n'))
                  : true)))
            {
               next_nonempty_line = nxt_len; // first nonwhitespace char in the next line
            }
         }

         /*
          * see if we should fold up; usually that'd be a YES, but there are a few
          * situations where folding/reflowing by merging lines is frowned upon:
          *
          * - ASCII art in the comments (most often, these are drawings done in +-\/|.,*)
          *
          * - Doxygen/JavaDoc/etc. parameters: these often start with \ or @, at least
          *   something clearly non-alphanumeric (you see where we're going with this?)
          *
          * - bullet lists that are closely spaced: bullets are always non-alphanumeric
          *   characters, such as '-' or '+' (or, oh horor, '*' - that's bloody ambiguous
          *   to parse :-( ... with or without '*' comment start prefix, that's the
          *   question, then.)
          *
          * - semi-HTML formatted code, e.g. <pre>...</pre> comment sections (NDoc, etc.)
          *
          * - New lines which form a new paragraph without there having been added an
          *   extra empty line between the last sentence and the new one.
          *   A bit like this, really; so it is opportune to check if the last line ended
          *   in a terminal (that would be the set '.:;!?') and the new line starts with
          *   a capital.
          *   Though new lines starting with comment delimiters, such as '(', should be
          *   pulled up.
          *
          * So it bores down to this: the only folding (& reflowing) that's going to happen
          * is when the next line starts with an alphanumeric character AND the last
          * line didn't end with an non-alphanumeric character, except: ',' AND the next
          * line didn't start with a '*' all of a sudden while the previous one didn't
          * (the ambiguous '*'-for-bullet case!)
          */
         if ((prev_nonempty_line >= 0) && (next_nonempty_line >= 0) &&
             (((unc_isalnum(line[prev_nonempty_line]) ||
                strchr(",)]", line[prev_nonempty_line])) &&
               (unc_isalnum(cmt_str[next_nonempty_line]) ||
                strchr("([", cmt_str[next_nonempty_line]))) ||
              (('.' == line[prev_nonempty_line]) &&    // dot followed by non-capital is NOT a new sentence start
               unc_isupper(cmt_str[next_nonempty_line]))) &&
             !star_is_bullet)
         {
            // rewind the line to the last non-alpha:
            line_len = prev_nonempty_line + 1;
            // roll the current line forward to the first non-alpha:
            cmt_str   += next_nonempty_line;
            remaining -= next_nonempty_line;
            // override the NL and make it a single whitespace:
            ch = ' ';
         }
      }

      line[line_len++] = ch;

      /* If we just hit an end of line OR we just hit end-of-comment... */
      if ((ch == '\n') || (remaining == 0))
      {
         line_count++;

         /* strip trailing tabs and spaces before the newline */
         if (ch == '\n')
         {
            nl_end = true;
            line_len--;
            cmt_trim_whitespace(line_len, line, pc->flags & PCF_IN_PREPROC);
         }
         line[line_len] = 0;

         //LOG_FMT(LSYS, "[%3d]%s\n", ccol, line);

         if (line_count == 1)
         {
            /* this is the first line - add unchanged */
            add_comment_text(line, line_len, cmt, false);
            if (nl_end)
            {
               add_char('\n');
            }
         }
         else
         {
            /* This is not the first line, so we need to indent to the
             * correct column. Each line is indented 0 or more spaces.
             */
            ccol -= col_diff;
            if (ccol < (cmt_col + 3))
            {
               ccol = cmt_col + 3;
            }

            if (line_len == 0)
            {
               /* Empty line - just a '\n' */
               if (cpd.settings[UO_cmt_star_cont].b)
               {
                  cmt.column = cmt_col + cpd.settings[UO_cmt_sp_before_star_cont].n;
                  cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                  if (cmt.xtra_indent)
                  {
                     add_char(' ');
                  }
                  add_text(cmt.cont_text);
               }
               add_char('\n');
            }
            else
            {
               /* If this doesn't start with a '*' or '|'.
                * '\name' is a common parameter documentation thing.
                */
               if (cpd.settings[UO_cmt_indent_multi].b &&
                   (line[0] != '*') && (line[0] != '|') && (line[0] != '#') &&
                   ((line[0] != '\\') || unc_isalpha(line[1])) && (line[0] != '+'))
               {
                  int start_col = cmt_col + cpd.settings[UO_cmt_sp_before_star_cont].n;

                  if (cpd.settings[UO_cmt_star_cont].b)
                  {
                     cmt.column = start_col;
                     cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                     if (cmt.xtra_indent)
                     {
                        add_char(' ');
                     }
                     add_text(cmt.cont_text);
                     output_to_column(ccol + cpd.settings[UO_cmt_sp_after_star_cont].n,
                                      false);
                  }
                  else
                  {
                     cmt.column = ccol;
                     cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                  }
               }
               else
               {
                  cmt.column = cmt_col + cpd.settings[UO_cmt_sp_before_star_cont].n;
                  cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
                  if (cmt.xtra_indent)
                  {
                     add_char(' ');
                  }

                  int idx;

                  idx = cmt_parse_lead(line, (remaining == 0));
                  if (idx > 0)
                  {
                     memcpy(lead, line, idx);
                     lead[idx] = 0;
                     cmt.cont_text = lead;
                  }
                  else
                  {
                     add_text(cmt.cont_text);
                  }
               }

               add_comment_text(line, line_len, cmt, false);
               if (nl_end)
               {
                  add_text("\n");
               }
            }
         }
         line_len = 0;
         ccol     = 1;
      }
   }
   delete[] line;
}


/**
 * Output a multiline comment without any reformatting other than shifting
 * it left or right to get the column right.
 * Oh, and trim trailing whitespace.
 */
static void output_comment_multi_simple(chunk_t *pc)
{
   int        cmt_col = pc->column;
   const char *cmt_str;
   int        remaining;
   char       ch;
   char       *line = new char[1024 + pc->len()];
   int        line_len;
   int        line_count = 0;
   int        ccol;
   int        col_diff = 0;
   bool       nl_end   = false;
   cmt_reflow cmt;

   output_cmt_start(cmt, pc);

   if (chunk_is_newline(chunk_get_prev(pc)))
   {
      /* The comment should be indented correctly */
      cmt_col  = pc->column;
      col_diff = pc->orig_col - pc->column;
   }
   else
   {
      /* The comment starts after something else */
      cmt_col  = pc->orig_col;
      col_diff = 0;
   }

   ccol      = pc->column;
   remaining = pc->len();
   cmt_str   = pc->text();
   line_len  = 0;
   while (remaining > 0)
   {
      ch = *cmt_str;
      cmt_str++;
      remaining--;

      /* handle the CRLF and CR endings. convert both to LF */
      if (ch == '\r')
      {
         ch = '\n';
         if (*cmt_str == '\n')
         {
            cmt_str++;
            remaining--;
         }
      }

      /* Find the start column */
      if (line_len == 0)
      {
         nl_end = false;
         if (ch == ' ')
         {
            ccol++;
            continue;
         }
         else if (ch == '\t')
         {
            ccol = calc_next_tab_column(ccol, cpd.settings[UO_input_tab_size].n);
            continue;
         }
         else
         {
            //LOG_FMT(LSYS, "%d] Text starts in col %d, col_diff=%d, real=%d\n",
            //        line_count, ccol, col_diff, ccol - col_diff);
         }
      }

      line[line_len++] = ch;

      /* If we just hit an end of line OR we just hit end-of-comment... */
      if ((ch == '\n') || (remaining == 0))
      {
         line_count++;

         /* strip trailing tabs and spaces before the newline */
         if (ch == '\n')
         {
            line_len--;
            nl_end = true;

            /* Say we aren't in a preproc to prevent changing any bs-nl */
            cmt_trim_whitespace(line_len, line, false);
         }
         line[line_len] = 0;

         if (line_count > 1)
         {
            ccol -= col_diff;
         }

         if (line_len > 0)
         {
            cmt.column = ccol;
            cmt_output_indent(cmt.brace_col, cmt.base_col, cmt.column);
            add_text_len(line, line_len);
         }
         if (nl_end)
         {
            add_char('\n');
         }
         line_len = 0;
         ccol     = 1;
      }
   }
   delete line;
}


/**
 * This renders the #if condition to a string buffer.
 */
static void generate_if_conditional_as_text(string& dst, chunk_t *ifdef)
{
   chunk_t *pc;
   int     column = -1;

   dst.erase();
   for (pc = ifdef; pc != NULL; pc = chunk_get_next(pc))
   {
      if (column == -1)
      {
         column = pc->column;
      }
      if ((pc->type == CT_NEWLINE) ||
          (pc->type == CT_COMMENT_MULTI) ||
          (pc->type == CT_COMMENT_CPP))
      {
         break;
      }
      else if (pc->type == CT_NL_CONT)
      {
         dst   += ' ';
         column = -1;
      }
      else if ((pc->type == CT_COMMENT) ||
               (pc->type == CT_COMMENT_EMBED))
      {
      }
      else // if (pc->type == CT_JUNK) || else
      {
         int spacing;

         for (spacing = pc->column - column; spacing > 0; spacing--)
         {
            dst += ' ';
            column++;
         }
         dst.append(pc->text(), pc->len());
         column += pc->len();
      }
   }
}


/*
 * See also it's preprocessor counterpart
 *   add_long_closebrace_comment
 * in braces.cpp
 *
 * Note: since this concerns itself with the preprocessor -- which is line-oriented --
 * it turns out that just looking at pc->pp_level is NOT the right thing to do.
 * See a --parsed dump if you don't believe this: an '#endif' will be one level
 * UP from the corresponding #ifdef when you look at the tokens 'ifdef' versus 'endif',
 * but it's a whole another story when you look at their CT_PREPROC ('#') tokens!
 *
 * Hence we need to track and seek matching CT_PREPROC pp_levels here, which complicates
 * things a little bit, but not much.
 */
void add_long_preprocessor_conditional_block_comment(void)
{
   chunk_t *pc;
   chunk_t *tmp;
   chunk_t *br_open;
   chunk_t *br_close;
   chunk_t *pp_start = NULL;
   chunk_t *pp_end   = NULL;
   int     nl_count;

   for (pc = chunk_get_head(); pc; pc = chunk_get_next_ncnl(pc))
   {
      /* just track the preproc level: */
      if (pc->type == CT_PREPROC)
      {
         pp_end = pp_start = pc;
      }

      if (pc->type != CT_PP_IF)
      {
         continue;
      }
#if 0
      if ((pc->flags & PCF_IN_PREPROC) != 0)
      {
         continue;
      }
#endif

      br_open  = pc;
      nl_count = 0;

      tmp = pc;
      while ((tmp = chunk_get_next(tmp)) != NULL)
      {
         /* just track the preproc level: */
         if (tmp->type == CT_PREPROC)
         {
            pp_end = tmp;
         }

         if (chunk_is_newline(tmp))
         {
            nl_count += tmp->nl_count;
         }
         else if ((pp_end->pp_level == pp_start->pp_level) &&
                  ((tmp->type == CT_PP_ENDIF) ||
                   (br_open->type == CT_PP_IF ? tmp->type == CT_PP_ELSE : 0)))
         {
            br_close = tmp;

            LOG_FMT(LPPIF, "found #if / %s section on lines %d and %d, nl_count=%d\n",
                    (tmp->type == CT_PP_ENDIF ? "#endif" : "#else"),
                    br_open->orig_line, br_close->orig_line, nl_count);

            /* Found the matching #else or #endif - make sure a newline is next */
            tmp = chunk_get_next(tmp);

            LOG_FMT(LPPIF, "next item type %d (is %s)\n",
                    (tmp ? tmp->type : -1), (tmp ? chunk_is_newline(tmp) ? "newline"
                                             : chunk_is_comment(tmp) ? "comment" : "other" : "---"));
            if ((tmp == NULL) || (tmp->type == CT_NEWLINE) /* chunk_is_newline(tmp) */)
            {
               int nl_min;

               if (br_close->type == CT_PP_ENDIF)
               {
                  nl_min = cpd.settings[UO_mod_add_long_ifdef_endif_comment].n;
               }
               else
               {
                  nl_min = cpd.settings[UO_mod_add_long_ifdef_else_comment].n;
               }

               const char *txt = !tmp ? "EOF" : ((tmp->type == CT_PP_ENDIF) ? "#endif" : "#else");
               LOG_FMT(LPPIF, "#if / %s section candidate for augmenting when over NL threshold %d != 0 (nl_count=%d)\n",
                       txt, nl_min, nl_count);

               if ((nl_min > 0) && (nl_count > nl_min)) /* nl_count is 1 too large at all times as #if line was counted too */
               {
                  /* determine the added comment style */
                  c_token_t style = (cpd.lang_flags & (LANG_CPP | LANG_CS)) ?
                                    CT_COMMENT_CPP : CT_COMMENT;

                  string str;
                  generate_if_conditional_as_text(str, br_open);

                  LOG_FMT(LPPIF, "#if / %s section over threshold %d (nl_count=%d) --> insert comment after the %s: %s\n",
                          txt, nl_min, nl_count, txt, str.c_str());

                  /* Add a comment after the close brace */
                  insert_comment_after(br_close, style, str.size(), str.c_str());
               }
            }

            /* checks both the #else and #endif for a given level, only then look further in the main loop */
            if (br_close->type == CT_PP_ENDIF)
            {
               break;
            }
         }
      }
   }
}
