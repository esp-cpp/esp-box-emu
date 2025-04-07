/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** memguard.c
**
** memory allocation wrapper routines
**
** NOTE: based on code (c) 1998 the Retrocade group
** $Id: memguard.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <noftypes.h>
#include <memguard.h>

/* undefine macro definitions, so we get real calls */
#undef malloc
#undef free
#undef strdup

#include <string.h>
#include <stdlib.h>
#include <log.h>


/* Maximum number of allocated blocks at any one time */
#define  MAX_BLOCKS        4096

/* Memory block structure */
typedef struct memblock_s
{
   void  *block_addr;
   int   block_size;
   char  *file_name;
   int   line_num;
} memblock_t;

char *_my_strdup(const char *string)
{
   char *temp;

   if (NULL == string)
      return NULL;

   /* will ASSERT for us */
   temp = (char *) _my_malloc(strlen(string) + 1);
   if (NULL == temp)
      return NULL;

   strcpy(temp, string);

   return temp;
}

/* check for orphaned memory handles */
void mem_checkleaks(void)
{
}

void mem_checkblocks(void)
{
}

/*
** $Log: memguard.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.23  2000/11/24 21:42:48  matt
** vc complaints
**
** Revision 1.22  2000/11/21 13:27:30  matt
** trash all newly allocated memory
**
** Revision 1.21  2000/11/21 13:22:30  matt
** memory guard shouldn't zero memory for us
**
** Revision 1.20  2000/10/28 14:01:53  matt
** memguard.h was being included in the wrong place
**
** Revision 1.19  2000/10/26 22:48:33  matt
** strdup'ing a NULL ptr returns NULL
**
** Revision 1.18  2000/10/25 13:41:29  matt
** added strdup
**
** Revision 1.17  2000/10/10 13:58:13  matt
** stroustrup squeezing his way in the door
**
** Revision 1.16  2000/10/10 13:03:54  matt
** Mr. Clean makes a guest appearance
**
** Revision 1.15  2000/09/18 02:06:48  matt
** -pedantic is your friend
**
** Revision 1.14  2000/08/11 01:45:48  matt
** hearing about no corrupt blocks every 10 seconds really was annoying
**
** Revision 1.13  2000/07/31 04:28:46  matt
** one million cleanups
**
** Revision 1.12  2000/07/24 04:31:07  matt
** mem_checkblocks now gives feedback
**
** Revision 1.11  2000/07/06 17:20:52  matt
** block manager space itself wasn't being freed - d'oh!
**
** Revision 1.10  2000/07/06 17:15:43  matt
** false isn't NULL, Neil... =)
**
** Revision 1.9  2000/07/05 23:10:01  neil
** It's a shame if the memguard segfaults
**
** Revision 1.8  2000/06/26 04:54:48  matt
** simplified and made more robust
**
** Revision 1.7  2000/06/12 01:11:41  matt
** cleaned up some error output for win32
**
** Revision 1.6  2000/06/09 15:12:25  matt
** initial revision
**
*/
