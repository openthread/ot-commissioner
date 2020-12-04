#include "file.hpp"

#if defined(_WIN32) || defined(WIN32)

#include <fcntl.h>
#include <io.h>
#include <share.h>

#else

#include <sys/file.h>
#include <unistd.h>

#endif

#include <errno.h>

namespace ot {

namespace os {

namespace file {

file_status file_read(std::string const &name, std::string &data)
{
    int fd = 0;
#if defined(_WIN32) || defined(WIN32)
    fd = _sopen(name.c_str(), _O_RDONLY, _SH_DENYRW, _S_IREAD | _S_IWRITE);
#else
    fd = open(name.c_str(), O_RDONLY, S_IWUSR | S_IRUSR);
#endif
    if (fd == -1)
    {
        if (errno == ENOENT)
        {
            return FS_NOT_EXISTS;
        }
        else if (errno == EACCES)
        {
            return FS_PERMISSION_ERROR;
        }
        return FS_ERROR;
    }

#if !defined(_WIN32) && !defined(WIN32)
    if (flock(fd, LOCK_EX) == -1)
    {
        close(fd);
        return FS_ERROR;
    }
#endif

    char buff[10 * 1024] = {0};
    int  read_bytes      = 0;
#if defined(_WIN32) || defined(WIN32)
    while ((read_bytes = _read(fd, buff, sizeof(buff) - 1)) > 0)
    {
#else
    while ((read_bytes = read(fd, buff, sizeof(buff) - 1)) > 0)
    {
#endif
        buff[read_bytes] = 0;
        data += std::string(buff);
    }

#if defined(_WIN32) || defined(WIN32)
    _close(fd);
#else
    close(fd);
#endif
    return FS_SUCCESS;
}

file_status file_write(std::string const &name, std::string const &data)
{
    int fd = 0;
#if defined(_WIN32) || defined(WIN32)
    fd = _sopen(name.c_str(), _O_CREAT | _O_WRONLY | _O_TRUNC, _SH_DENYRW, _S_IREAD | _S_IWRITE);
#else
    fd = open(name.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR);
#endif
    if (fd == -1)
    {
        if (errno == EACCES)
        {
            return FS_PERMISSION_ERROR;
        }
        return FS_ERROR;
    }

#if !defined(_WIN32) && !defined(WIN32)
    if (flock(fd, LOCK_EX) == -1)
    {
        close(fd);
        return FS_ERROR;
    }
#endif

#if defined(_WIN32) || defined(WIN32)
    _write(fd, data.c_str(), data.size());
    _close(fd);
#else
    write(fd, data.c_str(), data.size());
    close(fd);
#endif
    return FS_SUCCESS;
}

} // namespace file

} // namespace os

} // namespace ot
