
BOOL immediatlyReady = [self ensureResource: mutableResources[0]
                           existsInDirectoryAtPath: mutablePaths[0]
                                         queueMode: mode
                                 completionHandler: completionHandler
                                      errorHandler: errorHandler];

[myObject doFooWith1: arg1
               name1: arg2            // some lines with >1 arg
              error1: arg3];

[myObject doFooWith2: arg4
               name2: arg5
              error2: arg6];

[myObject doFooWith3: arg7
               name3: arg8 // aligning keywords instead of colons
              error3: arg9];

[myObject doithereguysA: argA
      reallylongargname: argB
                another: argC];
