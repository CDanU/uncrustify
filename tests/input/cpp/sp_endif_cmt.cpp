#if defined(FOO)
void d0();
#else/* cmt0 */
void d1();
#endif  /* cmt1_0
           cmt1_1
         */

#if defined(Bar)
void d2();
#else/* cmt2_0
        cmt2_1
      */
void d3();
#endif  // cmt3
