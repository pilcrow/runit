/* Public domain addition to DJB API. */

#include <fcntl.h>
#include "fd.h"

int fd_dup(int fd)
{
  return fcntl(fd, F_DUPFD, 0);
}
