// 
//  revert.c
//  src/jbloader/helper/revert.c
//  
//  Created 30/04/2023
//  jbloader (helper)
//

#include <jbloader.h>

int revert_install() {
    if (init_info()) return -1;
    return jailbreak_obliterator();
}
