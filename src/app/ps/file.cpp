#include "file.hpp"

#include <sys/file.h>
#include <unistd.h>

#include <errno.h>

#include <memory>

namespace ot {

namespace os {

namespace file {

namespace {

file_status open_error(int err)
{
    if (err == ENOENT)
    {
        return FS_NOT_EXISTS;
    }
    else if (err == EACCES)
    {
        return FS_PERMISSION_ERROR;
    }
    return FS_ERROR;
}

const std::string &&basename(const std::string &path)
{
    std::string tmp_path = path;
    // Special pathes: "/", "..", "."
    while (tmp_path.size() && tmp_path.back() == '/')
    {
        tmp_path.pop_back();
    }
    auto pos = tmp_path.find_last_of('/');
    if (pos != std::string::npos)
    {
        return std::move(tmp_path.substr(0, pos));
    }
    return std::move(std::string());
}

file_status restore_path_recursive(const std::string &rel_dir)
{
    (void)rel_dir;
    return FS_ERROR;
}

} // namespace

file_status file_read(std::string const &name, std::string &data)
{
    int fd = open(name.c_str(), O_RDONLY, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        if (errno != ENOENT)
        {
            return open_error(errno);
        }
        fd = creat(name.c_str(), 0660);
        if (fd == -1)
        {
            if (errno != ENOENT)
            {
                return open_error(errno);
            }
            std::string containing_dir = basename(name);
            file_status result         = restore_path_recursive(containing_dir);
            if (result != FS_SUCCESS)
            {
                return result;
            }
            fd = creat(name.c_str(), 0660);
            if (fd == -1)
            {
                return open_error(errno);
            }
        }
    }

    if (flock(fd, LOCK_EX) == -1)
    {
        close(fd);
        return FS_ERROR;
    }

    char buff[10 * 1024] = {0};
    int  read_bytes      = 0;
    while ((read_bytes = read(fd, buff, sizeof(buff) - 1)) > 0)
    {
        buff[read_bytes] = 0;
        data += std::string(buff);
    }

    close(fd);
    return FS_SUCCESS;
}

file_status file_write(std::string const &name, std::string const &data)
{
    int fd = open(name.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR);
    if (fd == -1)
    {
        if (errno == EACCES)
        {
            return FS_PERMISSION_ERROR;
        }
        return FS_ERROR;
    }

    if (flock(fd, LOCK_EX) == -1)
    {
        close(fd);
        return FS_ERROR;
    }

    ssize_t written = 0;
    do
    {
        ssize_t result = write(fd, data.c_str() + written, data.size() - written);
        if (result == -1)
        {
            if ((errno == EAGAIN || errno == EINTR))
            {
                continue;
            }
            else
            {
                close(fd);
                return FS_ERROR;
            }
        }
        written += result;
    } while (written != (ssize_t)data.size());
    close(fd);
    return FS_SUCCESS;
}

} // namespace file

} // namespace os

} // namespace ot
