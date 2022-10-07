#include <paging.h>
#include "../kernel/common/memory.h"
#include <stdint.h>

Page_Table loader_page_table =
        {(Page_Table_Entry){},
         (Page_Table_Entry{0})};