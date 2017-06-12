/**
 * @file keywords.cpp
 * Manages the table of keywords.
 *
 * @author  Ben Gardner
 * @author  Guy Maurel since version 0.62 for uncrustify4Qt
 *          October 2015, 2016
 * @license GPL v2+
 */
#include "keywords.h"
#include "uncrustify_types.h"
#include "prototypes.h"
#include "char_table.h"
#include "args.h"
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <map>
#include <tuple>
#include <array>
#include "unc_ctype.h"
#include "uncrustify.h"

#include "string_view.hpp"

using namespace std;

/* Dynamic keyword map */
typedef map<string, c_token_t> dkwmap;
static dkwmap dkwm;


/**
 * interesting static keywords - keep sorted.
 * Table should include the Name, Type, and Language flags.
 */
// tag, type, lang_flags
typedef tuple<libcxx_strviewclone::string_view, c_token_t, int>              keywords_tuple_t;
typedef array<tuple<libcxx_strviewclone::string_view, c_token_t, int>, 251 > keywords_t;

static const constexpr keywords_t keywords
{{
   make_tuple( "@catch",           CT_CATCH,         LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@dynamic",         CT_OC_DYNAMIC,    LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@end",             CT_OC_END,        LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@finally",         CT_FINALLY,       LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@implementation",  CT_OC_IMPL,       LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@interface",       CT_OC_INTF,       LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@interface",       CT_CLASS,         LANG_JAVA                                                                   ),
   make_tuple( "@private",         CT_PRIVATE,       LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@property",        CT_OC_PROPERTY,   LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@protocol",        CT_OC_PROTOCOL,   LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@selector",        CT_OC_SEL,        LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@synthesize",      CT_OC_DYNAMIC,    LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "@throw",           CT_THROW,         LANG_OC                                                                     ),
   make_tuple( "@try",             CT_TRY,           LANG_OC | LANG_CPP | LANG_C                                                 ),
   make_tuple( "NS_ENUM",          CT_ENUM,          LANG_OC                                                                     ),
   make_tuple( "NS_OPTIONS",       CT_ENUM,          LANG_OC                                                                     ),
   make_tuple( "Q_EMIT",           CT_Q_EMIT,        LANG_CPP                                                                    ), // guy 2015-10-16
   make_tuple( "Q_FOREACH",        CT_FOR,           LANG_CPP                                                                    ), // guy 2015-09-23
   make_tuple( "Q_FOREVER",        CT_Q_FOREVER,     LANG_CPP                                                                    ), // guy 2015-10-18
   make_tuple( "Q_GADGET",         CT_Q_GADGET,      LANG_CPP                                                                    ), // guy 2016-05-04
   make_tuple( "Q_OBJECT",         CT_COMMENT_EMBED, LANG_CPP                                                                    ),
   make_tuple( "_Bool",            CT_TYPE,          LANG_CPP                                                                    ),
   make_tuple( "_Complex",         CT_TYPE,          LANG_CPP                                                                    ),
   make_tuple( "_Imaginary",       CT_TYPE,          LANG_CPP                                                                    ),
   make_tuple( "__DI__",           CT_DI,            LANG_C | LANG_CPP                                                           ), // guy 2016-03-11
   make_tuple( "__HI__",           CT_HI,            LANG_C | LANG_CPP                                                           ), // guy 2016-03-11
   make_tuple( "__QI__",           CT_QI,            LANG_C | LANG_CPP                                                           ), // guy 2016-03-11
   make_tuple( "__SI__",           CT_SI,            LANG_C | LANG_CPP                                                           ), // guy 2016-03-11
   make_tuple( "__asm__",          CT_ASM,           LANG_C | LANG_CPP                                                           ),
   make_tuple( "__attribute__",    CT_ATTRIBUTE,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "__block",          CT_QUALIFIER,     LANG_OC                                                                     ),
   make_tuple( "__const__",        CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "__except",         CT_CATCH,         LANG_C | LANG_CPP                                                           ),
   make_tuple( "__finally",        CT_FINALLY,       LANG_C | LANG_CPP                                                           ),
   make_tuple( "__inline__",       CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "__nothrow__",      CT_NOTHROW,       LANG_C | LANG_CPP                                                           ), // guy 2016-03-11
   make_tuple( "__restrict",       CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "__signed__",       CT_TYPE,          LANG_C | LANG_CPP                                                           ),
   make_tuple( "__thread",         CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "__traits",         CT_QUALIFIER,     LANG_D                                                                      ),
   make_tuple( "__try",            CT_TRY,           LANG_C | LANG_CPP                                                           ),
   make_tuple( "__typeof__",       CT_SIZEOF,        LANG_C | LANG_CPP                                                           ),
   make_tuple( "__volatile__",     CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "__word__",         CT_WORD_,         LANG_C | LANG_CPP                                                           ), // guy 2016-03-11
   make_tuple( "abstract",         CT_QUALIFIER,     LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA                        ),
   make_tuple( "add",              CT_GETSET,        LANG_CS                                                                     ),
   make_tuple( "alias",            CT_QUALIFIER,     LANG_D                                                                      ),
   make_tuple( "align",            CT_ALIGN,         LANG_D                                                                      ),
   make_tuple( "alignof",          CT_SIZEOF,        LANG_C | LANG_CPP                                                           ),
   make_tuple( "and",              CT_SBOOL,         LANG_C | LANG_CPP | FLAG_PP                                                 ),
   make_tuple( "and_eq",           CT_SASSIGN,       LANG_C | LANG_CPP                                                           ),
   make_tuple( "as",               CT_AS,            LANG_CS | LANG_VALA                                                         ),
   make_tuple( "asm",              CT_ASM,           LANG_C | LANG_CPP | LANG_D                                                  ),
   make_tuple( "asm",              CT_PP_ASM,        LANG_ALL | FLAG_PP                                                          ),
   make_tuple( "assert",           CT_ASSERT,        LANG_JAVA                                                                   ),
   make_tuple( "assert",           CT_FUNCTION,      LANG_D | LANG_PAWN                                                          ), // PAWN
   make_tuple( "assert",           CT_PP_ASSERT,     LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "auto",             CT_TYPE,          LANG_C | LANG_CPP | LANG_D                                                  ),
   make_tuple( "base",             CT_BASE,          LANG_CS | LANG_VALA                                                         ),
   make_tuple( "bit",              CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "bitand",           CT_ARITH,         LANG_C | LANG_CPP                                                           ),
   make_tuple( "bitor",            CT_ARITH,         LANG_C | LANG_CPP                                                           ),
   make_tuple( "body",             CT_BODY,          LANG_D                                                                      ),
   make_tuple( "bool",             CT_TYPE,          LANG_CPP | LANG_CS | LANG_VALA                                              ),
   make_tuple( "boolean",          CT_TYPE,          LANG_JAVA | LANG_ECMA                                                       ),
   make_tuple( "break",            CT_BREAK,         LANG_ALL                                                                    ), // PAWN
   make_tuple( "byte",             CT_TYPE,          LANG_CS | LANG_D | LANG_JAVA | LANG_ECMA                                    ),
   make_tuple( "callback",         CT_QUALIFIER,     LANG_VALA                                                                   ),
   make_tuple( "case",             CT_CASE,          LANG_ALL                                                                    ), // PAWN
   make_tuple( "cast",             CT_D_CAST,        LANG_D                                                                      ),
   make_tuple( "catch",            CT_CATCH,         LANG_CPP | LANG_CS | LANG_VALA | LANG_D | LANG_JAVA | LANG_ECMA             ),
   make_tuple( "cdouble",          CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "cent",             CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "cfloat",           CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "char",             CT_CHAR,          LANG_PAWN                                                                   ), // PAWN
   make_tuple( "char",             CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "checked",          CT_QUALIFIER,     LANG_CS                                                                     ),
   make_tuple( "class",            CT_CLASS,         LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA             ),
   make_tuple( "compl",            CT_ARITH,         LANG_C | LANG_CPP                                                           ),
   make_tuple( "const",            CT_QUALIFIER,     LANG_ALL                                                                    ), // PAWN
   make_tuple( "const_cast",       CT_TYPE_CAST,     LANG_CPP                                                                    ),
   make_tuple( "constexpr",        CT_QUALIFIER,     LANG_CPP                                                                    ),
   make_tuple( "construct",        CT_CONSTRUCT,     LANG_VALA                                                                   ),
   make_tuple( "continue",         CT_CONTINUE,      LANG_ALL                                                                    ), // PAWN
   make_tuple( "creal",            CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "dchar",            CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "debug",            CT_DEBUG,         LANG_D                                                                      ),
   make_tuple( "debugger",         CT_DEBUGGER,      LANG_ECMA                                                                   ),
   make_tuple( "decltype",         CT_SIZEOF,        LANG_CPP                                                                    ),
   make_tuple( "default",          CT_DEFAULT,       LANG_ALL                                                                    ), // PAWN
   make_tuple( "define",           CT_PP_DEFINE,     LANG_ALL | FLAG_PP                                                          ), // PAWN
   make_tuple( "defined",          CT_DEFINED,       LANG_PAWN                                                                   ), // PAWN
   make_tuple( "defined",          CT_PP_DEFINED,    LANG_ALLC | FLAG_PP                                                         ),
   make_tuple( "delegate",         CT_DELEGATE,      LANG_CS | LANG_VALA | LANG_D                                                ),
   make_tuple( "delete",           CT_DELETE,        LANG_CPP | LANG_D | LANG_ECMA | LANG_VALA                                   ),
   make_tuple( "deprecated",       CT_QUALIFIER,     LANG_D                                                                      ),
   make_tuple( "do",               CT_DO,            LANG_ALL                                                                    ), // PAWN
   make_tuple( "double",           CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "dynamic_cast",     CT_TYPE_CAST,     LANG_CPP                                                                    ),
   make_tuple( "elif",             CT_PP_ELSE,       LANG_ALLC | FLAG_PP                                                         ),
   make_tuple( "else",             CT_ELSE,          LANG_ALL                                                                    ), // PAWN
   make_tuple( "else",             CT_PP_ELSE,       LANG_ALL | FLAG_PP                                                          ), // PAWN
   make_tuple( "elseif",           CT_PP_ELSE,       LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "emit",             CT_PP_EMIT,       LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "endif",            CT_PP_ENDIF,      LANG_ALL | FLAG_PP                                                          ), // PAWN
   make_tuple( "endinput",         CT_PP_ENDINPUT,   LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "endregion",        CT_PP_ENDREGION,  LANG_ALL | FLAG_PP                                                          ),
   make_tuple( "endscript",        CT_PP_ENDINPUT,   LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "enum",             CT_ENUM,          LANG_ALL                                                                    ), // PAWN
   make_tuple( "error",            CT_PP_ERROR,      LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "event",            CT_TYPE,          LANG_CS                                                                     ),
   make_tuple( "exit",             CT_FUNCTION,      LANG_PAWN                                                                   ), // PAWN
   make_tuple( "explicit",         CT_TYPE,          LANG_C | LANG_CPP | LANG_CS                                                 ),
   make_tuple( "export",           CT_EXPORT,        LANG_C | LANG_CPP | LANG_D | LANG_ECMA                                      ),
   make_tuple( "extends",          CT_QUALIFIER,     LANG_JAVA | LANG_ECMA                                                       ),
   make_tuple( "extern",           CT_EXTERN,        LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_VALA                            ),
   make_tuple( "false",            CT_WORD,          LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA                         ),
   make_tuple( "file",             CT_PP_FILE,       LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "final",            CT_QUALIFIER,     LANG_CPP | LANG_D | LANG_ECMA                                               ),
   make_tuple( "finally",          CT_FINALLY,       LANG_D | LANG_CS | LANG_VALA | LANG_ECMA | LANG_JAVA                        ),
   make_tuple( "flags",            CT_TYPE,          LANG_VALA                                                                   ),
   make_tuple( "float",            CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "for",              CT_FOR,           LANG_ALL                                                                    ), // PAWN
   make_tuple( "foreach",          CT_FOR,           LANG_CS | LANG_D | LANG_VALA                                                ),
   make_tuple( "foreach_reverse",  CT_FOR,           LANG_D                                                                      ),
   make_tuple( "forward",          CT_FORWARD,       LANG_PAWN                                                                   ), // PAWN
   make_tuple( "friend",           CT_FRIEND,        LANG_CPP                                                                    ),
   make_tuple( "function",         CT_FUNCTION,      LANG_D | LANG_ECMA                                                          ),
   make_tuple( "get",              CT_GETSET,        LANG_CS | LANG_VALA                                                         ),
   make_tuple( "goto",             CT_GOTO,          LANG_ALL                                                                    ), // PAWN
   make_tuple( "idouble",          CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "if",               CT_IF,            LANG_ALL                                                                    ), // PAWN
   make_tuple( "if",               CT_PP_IF,         LANG_ALL | FLAG_PP                                                          ), // PAWN
   make_tuple( "ifdef",            CT_PP_IF,         LANG_ALLC | FLAG_PP                                                         ),
   make_tuple( "ifloat",           CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "ifndef",           CT_PP_IF,         LANG_ALLC | FLAG_PP                                                         ),
   make_tuple( "implements",       CT_QUALIFIER,     LANG_JAVA | LANG_ECMA                                                       ),
   make_tuple( "implicit",         CT_QUALIFIER,     LANG_CS                                                                     ),
   make_tuple( "import",           CT_IMPORT,        LANG_D | LANG_JAVA | LANG_ECMA                                              ), // fudged to get indenting
   make_tuple( "import",           CT_PP_INCLUDE,    LANG_OC | FLAG_PP                                                           ), // ObjectiveC version of include
   make_tuple( "in",               CT_IN,            LANG_D | LANG_CS | LANG_VALA | LANG_ECMA | LANG_OC                          ),
   make_tuple( "include",          CT_PP_INCLUDE,    LANG_C | LANG_CPP | LANG_PAWN | FLAG_PP                                     ), // PAWN
   make_tuple( "inline",           CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "inout",            CT_QUALIFIER,     LANG_D                                                                      ),
   make_tuple( "instanceof",       CT_SIZEOF,        LANG_JAVA | LANG_ECMA                                                       ),
   make_tuple( "int",              CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "interface",        CT_CLASS,         LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA    ),
   make_tuple( "internal",         CT_QUALIFIER,     LANG_CS                                                                     ),
   make_tuple( "invariant",        CT_INVARIANT,     LANG_D                                                                      ),
   make_tuple( "ireal",            CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "is",               CT_SCOMPARE,      LANG_D | LANG_CS | LANG_VALA                                                ),
   make_tuple( "lazy",             CT_LAZY,          LANG_D                                                                      ),
   make_tuple( "line",             CT_PP_LINE,       LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "lock",             CT_LOCK,          LANG_CS | LANG_VALA                                                         ),
   make_tuple( "long",             CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "macro",            CT_D_MACRO,       LANG_D                                                                      ),
   make_tuple( "mixin",            CT_CLASS,         LANG_D                                                                      ), // may need special handling
   make_tuple( "module",           CT_D_MODULE,      LANG_D                                                                      ),
   make_tuple( "mutable",          CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "namespace",        CT_NAMESPACE,     LANG_CPP | LANG_CS | LANG_VALA                                              ),
   make_tuple( "native",           CT_NATIVE,        LANG_PAWN                                                                   ), // PAWN
   make_tuple( "native",           CT_QUALIFIER,     LANG_JAVA | LANG_ECMA                                                       ),
   make_tuple( "new",              CT_NEW,           LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_PAWN | LANG_VALA | LANG_ECMA ), // PAWN
   make_tuple( "not",              CT_SARITH,        LANG_C | LANG_CPP                                                           ),
   make_tuple( "not_eq",           CT_SCOMPARE,      LANG_C | LANG_CPP                                                           ),
   make_tuple( "null",             CT_TYPE,          LANG_CS | LANG_D | LANG_JAVA | LANG_VALA                                    ),
   make_tuple( "object",           CT_TYPE,          LANG_CS                                                                     ),
   make_tuple( "operator",         CT_OPERATOR,      LANG_CPP | LANG_CS | LANG_PAWN                                              ), // PAWN
   make_tuple( "or",               CT_SBOOL,         LANG_C | LANG_CPP | FLAG_PP                                                 ),
   make_tuple( "or_eq",            CT_SASSIGN,       LANG_C | LANG_CPP                                                           ),
   make_tuple( "out",              CT_QUALIFIER,     LANG_CS | LANG_D | LANG_VALA                                                ),
   make_tuple( "override",         CT_QUALIFIER,     LANG_CS | LANG_D | LANG_VALA                                                ),
   make_tuple( "package",          CT_PRIVATE,       LANG_D                                                                      ),
   make_tuple( "package",          CT_PACKAGE,       LANG_ECMA | LANG_JAVA                                                       ),
   make_tuple( "params",           CT_TYPE,          LANG_CS | LANG_VALA                                                         ),
   make_tuple( "pragma",           CT_PP_PRAGMA,     LANG_ALL | FLAG_PP                                                          ), // PAWN
   make_tuple( "private",          CT_PRIVATE,       LANG_ALLC                                                                   ), // not C
   make_tuple( "property",         CT_PP_PROPERTY,   LANG_CS | FLAG_PP                                                           ),
   make_tuple( "protected",        CT_PRIVATE,       LANG_ALLC                                                                   ), // not C
   make_tuple( "public",           CT_PRIVATE,       LANG_ALL                                                                    ), // PAWN // not C
   make_tuple( "readonly",         CT_QUALIFIER,     LANG_CS                                                                     ),
   make_tuple( "real",             CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "ref",              CT_QUALIFIER,     LANG_CS | LANG_VALA                                                         ),
   make_tuple( "region",           CT_PP_REGION,     LANG_ALL | FLAG_PP                                                          ),
   make_tuple( "register",         CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "reinterpret_cast", CT_TYPE_CAST,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "remove",           CT_GETSET,        LANG_CS                                                                     ),
   make_tuple( "restrict",         CT_QUALIFIER,     LANG_C | LANG_CPP                                                           ),
   make_tuple( "return",           CT_RETURN,        LANG_ALL                                                                    ), // PAWN
   make_tuple( "sbyte",            CT_TYPE,          LANG_CS                                                                     ),
   make_tuple( "scope",            CT_D_SCOPE,       LANG_D                                                                      ),
   make_tuple( "sealed",           CT_QUALIFIER,     LANG_CS                                                                     ),
   make_tuple( "section",          CT_PP_SECTION,    LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "set",              CT_GETSET,        LANG_CS | LANG_VALA                                                         ),
   make_tuple( "short",            CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "signal",           CT_PRIVATE,       LANG_VALA                                                                   ),
   make_tuple( "signals",          CT_PRIVATE,       LANG_CPP                                                                    ),
   make_tuple( "signed",           CT_TYPE,          LANG_C | LANG_CPP                                                           ),
   make_tuple( "sizeof",           CT_SIZEOF,        LANG_C | LANG_CPP | LANG_CS | LANG_VALA | LANG_PAWN                         ), // PAWN
   make_tuple( "sleep",            CT_SIZEOF,        LANG_PAWN                                                                   ), // PAWN
   make_tuple( "stackalloc",       CT_NEW,           LANG_CS                                                                     ),
   make_tuple( "state",            CT_STATE,         LANG_PAWN                                                                   ), // PAWN
   make_tuple( "static",           CT_QUALIFIER,     LANG_ALL                                                                    ), // PAWN
   make_tuple( "static_cast",      CT_TYPE_CAST,     LANG_CPP                                                                    ),
   make_tuple( "stock",            CT_STOCK,         LANG_PAWN                                                                   ), // PAWN
   make_tuple( "strictfp",         CT_QUALIFIER,     LANG_JAVA                                                                   ),
   make_tuple( "string",           CT_TYPE,          LANG_CS | LANG_VALA                                                         ),
   make_tuple( "struct",           CT_STRUCT,        LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_VALA                            ),
   make_tuple( "super",            CT_SUPER,         LANG_D | LANG_JAVA | LANG_ECMA                                              ),
   make_tuple( "switch",           CT_SWITCH,        LANG_ALL                                                                    ), // PAWN
   make_tuple( "synchronized",     CT_QUALIFIER,     LANG_D | LANG_ECMA                                                          ),
   make_tuple( "synchronized",     CT_SYNCHRONIZED,  LANG_JAVA                                                                   ),
   make_tuple( "tagof",            CT_TAGOF,         LANG_PAWN                                                                   ), // PAWN
   make_tuple( "template",         CT_TEMPLATE,      LANG_CPP | LANG_D                                                           ),
   make_tuple( "this",             CT_THIS,          LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA             ),
   make_tuple( "throw",            CT_THROW,         LANG_CPP | LANG_CS | LANG_VALA | LANG_D | LANG_JAVA | LANG_ECMA             ),
   make_tuple( "throws",           CT_QUALIFIER,     LANG_JAVA | LANG_ECMA | LANG_VALA                                           ),
   make_tuple( "transient",        CT_QUALIFIER,     LANG_JAVA | LANG_ECMA                                                       ),
   make_tuple( "true",             CT_WORD,          LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA                         ),
   make_tuple( "try",              CT_TRY,           LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_ECMA | LANG_VALA             ),
   make_tuple( "tryinclude",       CT_PP_INCLUDE,    LANG_PAWN | FLAG_PP                                                         ), // PAWN
   make_tuple( "typedef",          CT_TYPEDEF,       LANG_C | LANG_CPP | LANG_D | LANG_OC                                        ),
   make_tuple( "typeid",           CT_SIZEOF,        LANG_C | LANG_CPP | LANG_D                                                  ),
   make_tuple( "typename",         CT_TYPENAME,      LANG_CPP                                                                    ),
   make_tuple( "typeof",           CT_SIZEOF,        LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_VALA | LANG_ECMA                ),
   make_tuple( "ubyte",            CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "ucent",            CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "uint",             CT_TYPE,          LANG_CS | LANG_VALA | LANG_D                                                ),
   make_tuple( "ulong",            CT_TYPE,          LANG_CS | LANG_VALA | LANG_D                                                ),
   make_tuple( "unchecked",        CT_QUALIFIER,     LANG_CS                                                                     ),
   make_tuple( "undef",            CT_PP_UNDEF,      LANG_ALL | FLAG_PP                                                          ), // PAWN
   make_tuple( "union",            CT_UNION,         LANG_C | LANG_CPP | LANG_D                                                  ),
   make_tuple( "unittest",         CT_UNITTEST,      LANG_D                                                                      ),
   make_tuple( "unsafe",           CT_UNSAFE,        LANG_CS                                                                     ),
   make_tuple( "unsigned",         CT_TYPE,          LANG_C | LANG_CPP                                                           ),
   make_tuple( "ushort",           CT_TYPE,          LANG_CS | LANG_VALA | LANG_D                                                ),
   make_tuple( "using",            CT_USING,         LANG_CPP | LANG_CS | LANG_VALA                                              ),
   make_tuple( "var",              CT_TYPE,          LANG_VALA | LANG_ECMA                                                       ),
   make_tuple( "version",          CT_D_VERSION,     LANG_D                                                                      ),
   make_tuple( "virtual",          CT_QUALIFIER,     LANG_CPP | LANG_CS | LANG_VALA                                              ),
   make_tuple( "void",             CT_TYPE,          LANG_ALLC                                                                   ),
   make_tuple( "volatile",         CT_QUALIFIER,     LANG_C | LANG_CPP | LANG_CS | LANG_JAVA | LANG_ECMA                         ),
   make_tuple( "volatile",         CT_VOLATILE,      LANG_D                                                                      ),
   make_tuple( "wchar",            CT_TYPE,          LANG_D                                                                      ),
   make_tuple( "wchar_t",          CT_TYPE,          LANG_C | LANG_CPP                                                           ),
   make_tuple( "weak",             CT_QUALIFIER,     LANG_VALA                                                                   ),
   make_tuple( "when",             CT_WHEN,          LANG_CS                                                                     ),
   make_tuple( "while",            CT_WHILE,         LANG_ALL                                                                    ), // PAWN
   make_tuple( "with",             CT_D_WITH,        LANG_D | LANG_ECMA                                                          ),
   make_tuple( "xor",              CT_SARITH,        LANG_C | LANG_CPP                                                           ),
   make_tuple( "xor_eq",           CT_SASSIGN,       LANG_C | LANG_CPP                                                           )
}};
//static const constexp


/**
 * Compares two chunk_tag_t entries using strcmp on the strings
 *
 * @param p1   The 'left' entry
 * @param p2   The 'right' entry
 */
static int kw_compare(const void *p1, const void *p2);


/**
 * Backs up to the first string match in keywords.
 */
static const chunk_tag_t *kw_static_first(const chunk_tag_t *tag);





static int kw_compare(const void *p1, const void *p2)
{
   const chunk_tag_t *t1 = static_cast<const chunk_tag_t *>(p1);
   const chunk_tag_t *t2 = static_cast<const chunk_tag_t *>(p2);

   return(strcmp(t1->tag, t2->tag));
}


void init_keywords()
{
}


bool keywords_are_sorted(void)
{

   for (int idx = 1; idx < keywords.size(); idx++)
   {
      if (kw_compare(&keywords[idx - 1], &keywords[idx]) > 0)
      {
         fprintf(stderr, "%s: bad sort order at idx %d, words '%s' and '%s'\n",
                 __func__, idx - 1,
					  std::get<0>(keywords[idx - 1]).data(),
					  std::get<0>(keywords[idx]).data());
         log_flush(true);
         cpd.error_count++;
         return(false);
      }
   }

   return(true);
}


void add_keyword(const char *tag, c_token_t type)
{
   string ss = tag;

   /* See if the keyword has already been added */
   dkwmap::iterator it = dkwm.find(ss);

   if (it != dkwm.end())
   {
      LOG_FMT(LDYNKW, "%s: changed '%s' to %d\n", __func__, tag, type);
      (*it).second = type;
      return;
   }

   /* Insert the keyword */
   dkwm.insert(dkwmap::value_type(ss, type));
   LOG_FMT(LDYNKW, "%s: added '%s' as %d\n", __func__, tag, type);
}


void remove_keyword(const string &tag)
{
   if (tag.empty())
   {
      return;
   }

   /* See if the keyword exists in the map */
   dkwmap::iterator it = dkwm.find(tag);
   if (it == dkwm.end())
   {
      return;
   }

   /* Remove the keyword */
   dkwm.erase(it);
   LOG_FMT(LDYNKW, "%s: removed '%s'\n", __func__, tag.c_str());
}




static const keywords_tuple_t *kw_static_match(const chunk_tag_t *tag)
{
   bool in_pp = ((cpd.in_preproc != CT_NONE) && (cpd.in_preproc != CT_PP_DEFINE));

   for (const auto kw_t : keywords)
   {
      bool pp_iter = (get<2>(kw_t) & FLAG_PP) != 0;

      if (in_pp == pp_iter
      	 && (cpd.lang_flags & get<1>(kw_t))
          && strcmp(get<0>(kw_t).data(), tag->tag) == 0)
      {
         return(&kw_t);
      }
   }
   return(nullptr);
}


c_token_t find_keyword_type(const char *word, size_t len)
{
   if (len <= 0)
   {
      return(CT_NONE);
   }

   /* check the dynamic word list first */
   string           ss(word, len);
   dkwmap::iterator it = dkwm.find(ss);
   if (it != dkwm.end())
   {
      return((*it).second);
   }

   chunk_tag_t key;
   key.tag = ss.c_str();

   /* check the static word list */

   auto* p_ret = kw_static_match(&key);

   return((p_ret != nullptr) ? std::get<1>(*p_ret) : CT_WORD);
}


int load_keyword_file(const char *filename)
{
   FILE *pf = fopen(filename, "r");

   if (pf == nullptr)
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n",
              __func__, filename, strerror(errno), errno);
      cpd.error_count++;
      return(EX_IOERR);
   }

#define MAXLENGTHOFLINE    256
#define NUMBEROFARGS       2
   // maximal length of a line in the file
   char   buf[MAXLENGTHOFLINE];
   char   *args[NUMBEROFARGS];
   size_t line_no = 0;
   while (fgets(buf, MAXLENGTHOFLINE, pf) != nullptr)
   {
      line_no++;

      /* remove comments */
      char *ptr;
      if ((ptr = strchr(buf, '#')) != nullptr)
      {
         // a comment line
         *ptr = 0;
         // don't use the rest of the line
      }

      size_t argc = Args::SplitLine(buf, args, NUMBEROFARGS);

      if (argc > 0)
      {
         if ((argc == 1) && CharTable::IsKw1(*args[0]))
         {
            add_keyword(args[0], CT_TYPE);
         }
         else
         {
            LOG_FMT(LWARN, "%s:%zu Invalid line (starts with '%s')\n",
                    filename, line_no, args[0]);
            cpd.error_count++;
         }
      }
      else
      {
         // the line is empty
         continue;
      }
   }

   fclose(pf);
   return(EX_OK);
} // load_keyword_file


void print_keywords(FILE *pfile)
{
   for (const auto &keyword_pair : dkwm)
   {
      c_token_t tt = keyword_pair.second;
      if (tt == CT_TYPE)
      {
         fprintf(pfile, "type %*.s%s\n",
                 MAX_OPTION_NAME_LEN - 4, " ", keyword_pair.first.c_str());
      }
      else if (tt == CT_MACRO_OPEN)
      {
         fprintf(pfile, "macro-open %*.s%s\n",
                 MAX_OPTION_NAME_LEN - 11, " ", keyword_pair.first.c_str());
      }
      else if (tt == CT_MACRO_CLOSE)
      {
         fprintf(pfile, "macro-close %*.s%s\n",
                 MAX_OPTION_NAME_LEN - 12, " ", keyword_pair.first.c_str());
      }
      else if (tt == CT_MACRO_ELSE)
      {
         fprintf(pfile, "macro-else %*.s%s\n",
                 MAX_OPTION_NAME_LEN - 11, " ", keyword_pair.first.c_str());
      }
      else
      {
         const char *tn = get_token_name(tt);

         fprintf(pfile, "set %s %*.s%s\n", tn,
                 int(MAX_OPTION_NAME_LEN - (4 + strlen(tn))), " ", keyword_pair.first.c_str());
      }
   }
}


void clear_keyword_file(void)
{
   dkwm.clear();
}


pattern_class_e get_token_pattern_class(c_token_t tok)
{
   switch (tok)
   {
   case CT_IF:
   case CT_ELSEIF:
   case CT_SWITCH:
   case CT_FOR:
   case CT_WHILE:
   case CT_SYNCHRONIZED:
   case CT_USING_STMT:
   case CT_LOCK:
   case CT_D_WITH:
   case CT_D_VERSION_IF:
   case CT_D_SCOPE_IF:
      return(pattern_class_e::PBRACED);

   case CT_ELSE:
      return(pattern_class_e::ELSE);

   case CT_DO:
   case CT_TRY:
   case CT_FINALLY:
   case CT_BODY:
   case CT_UNITTEST:
   case CT_UNSAFE:
   case CT_VOLATILE:
   case CT_GETSET:
      return(pattern_class_e::BRACED);

   case CT_CATCH:
   case CT_D_VERSION:
   case CT_DEBUG:
      return(pattern_class_e::OPBRACED);

   case CT_NAMESPACE:
      return(pattern_class_e::VBRACED);

   case CT_WHILE_OF_DO:
      return(pattern_class_e::PAREN);

   case CT_INVARIANT:
      return(pattern_class_e::OPPAREN);

   default:
      return(pattern_class_e::NONE);
   } // switch
}    // get_token_pattern_class
