#if (defined(WIN32))
# include <windows.h>
# include <stdio.h>
#else
#if (defined(MACOS) || defined(LINUX))
# include <sys/xattr.h>
#endif
#if (defined(SUN) || defined(LINUX))
#include <sys/types.h>
#include <sys/stat.h>
#endif
#if (defined(SUN))
#include <fcntl.h>
/* DIR */
#include <dirent.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#if (defined(WIN32))
#define log_event	printf
#define log_error(mess)	printf("Error %s", (mess))
#else
#define log_event(...)	fprintf(stderr, __VA_ARGS__)
#define log_error	perror
#endif

int show_xattr(char *file, char *name)
{

  int		size;
  void *	result;
#if (defined(WIN32))
  int		retos;
  int		nb;
  HANDLE	hfind;
  WIN32_FIND_STREAM_DATA  FindStream;
  HANDLE	hStream;
  char 		stream_name[1024];
  char 		full_stream_name[1024];
  WCHAR		stream_nameW[1024];
  WCHAR		full_stream_nameW[1024];
  WCHAR		file_nameW[1024];
  DWORD		n_read;
  size_t	returnValue;
#else
  size_t	read_size;
#if (defined(MACOS) || defined(LINUX))
#ifdef LINUX
  int		retos;
  struct stat	st;
#endif
#ifdef MACOS
  int		options;
#endif
#elif (defined(SUN))
  int		retos;
  int		xattr_fd;
  int		xattr_list_fd;
  struct stat	stat_buf;
#endif
#endif

#if (defined(WIN32))

  mbstowcs_s(
	&returnValue,
	file_nameW,
	1024,
	file,
	strlen(file)+1);

  mbstowcs_s(
	&returnValue,
	stream_nameW,
	1024,
	name,
	strlen(name)+1);

  sprintf(full_stream_name, ":%s:$DATA", name);

  mbstowcs_s(
	&returnValue,
	full_stream_nameW,
	1024,
	full_stream_name,
	strlen(full_stream_name)+1);

  nb = 1;

  hfind = FindFirstStreamW(
		file_nameW,
		FindStreamInfoStandard,
		&FindStream,
		0);
  if (hfind == INVALID_HANDLE_VALUE) {
    log_event("Unable to get %s file first stream stat, retos=%d\n", file, GetLastError());
    return 0;
  }

  while (hfind != INVALID_HANDLE_VALUE) {

    if (wcscmp(FindStream.cStreamName, full_stream_nameW) == 0) {
      size = FindStream.StreamSize.LowPart;
      break;
    }

    nb ++;

    retos = FindNextStreamW(
		hfind,
		&FindStream);
    if (retos == FALSE) {
      retos = GetLastError();
      if (retos == ERROR_HANDLE_EOF) {
        log_event("Unable to find the %s file next stream, %d stream listed, retos=%d\n", file, nb, GetLastError());
      }
      else {
        log_event("Unable to get %s file next stream stat, nb=%d, retos=%d\n", file, nb, GetLastError());
      }
      FindClose(hfind);
      return 0;
    }

  }

  FindClose(hfind);

  sprintf(stream_name, "%s:%s", file, name);

  hStream = CreateFileA(
		stream_name,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		0);
  if (hStream == INVALID_HANDLE_VALUE) {
    log_event("Unable to open %s stream, retos=%d\n", stream_name, GetLastError());
    return 0;
  }

  result = malloc(size);
  if (NULL != result) {

    retos = ReadFile(
		hStream,
		result,
		size,
		&n_read,
		NULL);
    if (retos == FALSE) {
      log_event("Unable to read %s data in %s stream, retos=%d",
                size, stream_name, GetLastError());
      CloseHandle(hStream);
      return 0;
    }
    else {

      int i;

      for (i=0; i < size; i++) {
        printf("%c", ((char *)(result))[i]);
      }

      free(result);

    }

  }

  CloseHandle(hStream);

  return 1;

#else

#if (defined(MACOS) || defined(LINUX))

#ifdef MACOS
  options = XATTR_NOFOLLOW;
#endif

#if defined(LINUX)

  retos = lstat(file, &st);
  if (retos == -1) {
    return 0;
  }

  if (S_ISLNK(st.st_mode)) {

    size = lgetxattr(
		  file,
		  name,
		  NULL,
		  0
		  );

  }
  else {

#endif

    size = getxattr(
		  file,
		  name,
		  NULL,
		  0
#ifdef MACOS
		  ,0,
		  options
#endif
		  );

#if defined(LINUX)
  }
#endif

  if (size < 0) {
    return 0;
  }

  result = malloc(size);
  if (NULL != result) {

#if defined(LINUX)

    if (S_ISLNK(st.st_mode)) {
  
    read_size = lgetxattr(
		  file,
		  name,
		  result,
		  size
		  );

    }
    else {

#endif

      read_size = getxattr(
			file,
			name,
			result,
			size
#ifdef MACOS
			,0,
			options
#endif
			);

#if defined(LINUX)
  }
#endif
    if (read_size >= 0) {
      write(1, result, read_size);
      free(result);
      return 1;
    }

    free(result);

  }

#elif (defined(SUN))

  xattr_list_fd = attropen(file, ".", O_RDONLY);
  if (xattr_list_fd == -1) {
    return 0;
  }

  xattr_fd = openat(xattr_list_fd, name, O_RDONLY);
  if (xattr_fd == -1) {
    close(xattr_list_fd);
    return 0;
  }
    
  retos = fstat(
                xattr_fd,
                &stat_buf);
  if (retos == -1) {
    close(xattr_fd);
    close(xattr_list_fd);
    return 0;
  }

  size = stat_buf.st_size;

  result = malloc(size);
  if (NULL != result) {

    read_size = read(
			xattr_fd,
			result,
			size);
    if (read_size >= 0) {
      write(1, result, read_size);
      free(result);
      close(xattr_fd);
      close(xattr_list_fd);
      return 1;
    }

    free(result);
    close(xattr_fd);
    close(xattr_list_fd);

  }

#endif

  return 0;

#endif

}

int main(int argc, char *argv[]) {
    
  int		code;
  size_t	size;
#if (defined(WIN32))
  int		retos;
  int		nb;
  HANDLE	hfind;
  WIN32_FIND_STREAM_DATA  FindStream;
  size_t	returnValue;
  char 		stream_name[1024];
  char 		full_stream_name[1024];
  WCHAR		file_nameW[1024];
  HANDLE	hStream;
  DWORD		n_written;
#else
#if defined(LINUX)
  int		retos;
  struct stat	st;
#endif
#if (defined(MACOS) || defined(LINUX))
  int		options;
  char *	buf;
  char *	end;
#elif (defined(SUN))
  int		xattr_list_fd;
  int		xattr_fd;
  int           attrdirfd;
  struct dirent *p_dirent;
  DIR *  dd = (DIR *)NULL;
#endif
#endif

  if (argc < 3) {
    log_event("Usage: %s [l | a | r attr | w attr val] file\n", 
            argv[0]);
    return 1;
  }
    
#if (defined(MACOS) || defined(LINUX))

  options = 0;
#ifdef MACOS
  options += XATTR_NOFOLLOW;
#endif

#endif

  if (argv[1][0] == 'r') {

    /* Read mode */
    if (argc != 4) {
      log_event("Wrong number of arguments\n");
      return 1;
    }

    if (show_xattr(argv[3], argv[2]))
      return 0;

    log_error(argv[0]);
    return 1;

  } 
  else if (argv[1][0] == 'w') {

    /* Write mode */
    if (argc != 5) {
      log_event("Wrong number of arguments\n");
      return 1;
    }

#if (defined(WIN32))

  sprintf(stream_name, "%s:%s", argv[4], argv[2]);

  hStream = CreateFileA(
		stream_name,
		GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_ALWAYS,
		0,
		0);
  if (hStream == INVALID_HANDLE_VALUE) {
    log_event("Unable to open %s stream, retos=%d\n", stream_name, GetLastError());
    return 1;
  }

  retos = WriteFile(
		hStream,
		argv[3],
		strlen(argv[3])+1,
		&n_written,
		NULL);
  if (retos == FALSE) {
    log_event("Unable to write %s data in %s stream, retos=%d",
              argv[3], stream_name, GetLastError());
    CloseHandle(hStream);
    return 1;
  }

  CloseHandle(hStream);

#else

#if (defined(MACOS) || defined(LINUX))

#if defined(LINUX)

    retos = lstat(argv[4], &st);
    if (retos == -1) {
      log_error(argv[0]);
      return 1;
    }

    if (S_ISLNK(st.st_mode)) {

      code = lsetxattr(
		argv[4],
		argv[2],
		argv[3],
		strlen(argv[3])+1,
		options
		);

    }
    else {

#endif

    code = setxattr(
		argv[4],
		argv[2],
		argv[3],
		strlen(argv[3])+1,
#ifdef MACOS
		0,
#endif
		options
		);

#if defined(LINUX)
    }
#endif

    if (code < 0) {
      log_error(argv[0]);
      return 1;
    }

#elif (defined(SUN))

    xattr_list_fd = open(argv[4], O_RDONLY, 0);
    if (xattr_fd == -1) {
      log_error(argv[0]);
      return 1;
    }

    xattr_fd = openat(xattr_list_fd, argv[2], O_CREAT|O_RDWR|O_XATTR, (mode_t)0777);
    if (xattr_fd == -1) {
      close(xattr_list_fd);
      log_error(argv[0]);
      return 1;
    }

    code = write(xattr_fd, argv[3], strlen(argv[3])+1);
    if (code == -1) {
      close(xattr_fd);
      close(xattr_list_fd);
      log_error(argv[0]);
      return 1;
    }

    close(xattr_fd);
    close(xattr_list_fd);

#endif

#endif

    return 0;

  }
  else if (argv[1][0] == 'l') {

    if (argc != 3) {
      log_event("Wrong number of arguments\n");
      return 1;
    }

#if (defined(WIN32))

  mbstowcs_s(
	&returnValue,
	file_nameW,
	1024,
	argv[2],
	strlen(argv[2])+1);

  nb = 1;

  hfind = FindFirstStreamW(
		file_nameW,
		FindStreamInfoStandard,
		&FindStream,
		0);
  if (hfind == INVALID_HANDLE_VALUE) {
    log_event("Unable to get %s file first stream stat, retos=%d\n", argv[2], GetLastError());
    return 1;
  }

  while (hfind != INVALID_HANDLE_VALUE) {

    /* Display name and data	*/
    /* of current stream	*/

    /* Filter the first real data stream	*/

    if (nb != 1) {
    
      wcstombs_s(
		&returnValue,
		full_stream_name,
		1024,
		FindStream.cStreamName,
		(wcslen(FindStream.cStreamName)+1)*sizeof(char));

      { /* extract xattr name from :name:$DATA */

        char *	p;

        p = strchr(full_stream_name+1, ':');

        strncpy(stream_name, full_stream_name+1, p-(full_stream_name+1));
        stream_name[p-(full_stream_name+1)] = '\0';

      }
 
      printf("%s\n", stream_name);

    }

    /* Next stream */

    nb ++;

    retos = FindNextStreamW(
		hfind,
		&FindStream);
    if (retos == FALSE) {
      retos = GetLastError();
      if (retos == ERROR_HANDLE_EOF) {
        break;
      }
      else {
        log_event("Unable to get %s file next stream stat, nb = %d, retos=%d\n", argv[2], nb, GetLastError());
      }
      FindClose(hfind);
      return 1;
    }

  }

  FindClose(hfind);

#else

#if (defined(MACOS) || defined(LINUX))

#if defined(LINUX)

    retos = lstat(argv[2], &st);
    if (retos == -1) {
      log_error(argv[0]);
      return 1;
    }

    if (S_ISLNK(st.st_mode)) {

      size = llistxattr(
		argv[2],
		NULL,
		0
		);

    }
    else {

#endif

      size = listxattr(
		argv[2],
		NULL,
		0
#ifdef MACOS
		,options
#endif
		);

#if defined(LINUX)
    }
#endif

    if (size == 0) {
      return 0;
    }

    if (size < 0) {
      log_error(argv[0]);
      return 1;
    }

    buf = (char *)malloc(size);

#if defined(LINUX)

    if (S_ISLNK(st.st_mode)) {

      size = llistxattr(
		argv[2],
		buf,
		size
		);
    }
    else {

#endif

      size = listxattr(
		argv[2],
		buf,
		size
#ifdef MACOS
		,options
#endif
		);

#if defined(LINUX)
    }
#endif

    if (size < 0) {
      log_error(argv[0]);
      return 1;
    }

    end = buf + size;

    while(buf < end) {
      printf("%s\n", buf);
      buf += strlen(buf) + 1;
    }

#elif (defined(SUN))

  attrdirfd = attropen(argv[2], ".", O_RDONLY);
  if (attrdirfd == -1) {
    log_error(argv[0]);
    return 1;
  }

  dd = fdopendir(attrdirfd);
  if (dd == (DIR *)NULL) {
    close(attrdirfd);
    log_error(argv[0]);
    return 1;
  }
    
  while (p_dirent = readdir(dd)) {

    if ((strcmp(p_dirent->d_name, ".")  == 0) ||
        (strcmp(p_dirent->d_name, "..") == 0)) {
      continue;
    }
    
    printf("%s\n", p_dirent->d_name);
  
  }
    
  closedir(dd);
  close(attrdirfd);

#endif

#endif

    return 0;

  }
  else if (argv[1][0] == 'a') {

    if (argc != 3) {
      log_event("Wrong number of arguments\n");
      return 1;
    }

#if (defined(WIN32))

  mbstowcs_s(
	&returnValue,
	file_nameW,
	1024,
	argv[2],
	strlen(argv[2])+1);

  nb = 1;

  hfind = FindFirstStreamW(
		file_nameW,
		FindStreamInfoStandard,
		&FindStream,
		0);
  if (hfind == INVALID_HANDLE_VALUE) {
    log_event("Unable to get %s file first stream stat, retos=%d\n", argv[2], GetLastError());
    return 1;
  }

  while (hfind != INVALID_HANDLE_VALUE) {

    /* Display name and data	*/
    /* of current stream	*/

    /* Filter the first real data stream	*/

    if (nb != 1) {
    
      wcstombs_s(
		&returnValue,
		full_stream_name,
		1024,
		FindStream.cStreamName,
		(wcslen(FindStream.cStreamName)+1)*sizeof(char));

      { /* extract xattr name from :name:$DATA */

        char *	p;

        p = strchr(full_stream_name+1, ':');

        strncpy(stream_name, full_stream_name+1, p-(full_stream_name+1));
        stream_name[p-(full_stream_name+1)] = '\0';

      }
 
      printf("%s: \"", stream_name);
      fflush(stdout);

      if (!show_xattr(argv[2], stream_name)) {
        log_error(argv[0]);
        return 1;
      }

      printf("\"\n");

    }

    /* Next stream */

    nb ++;

    retos = FindNextStreamW(
		hfind,
		&FindStream);
    if (retos == FALSE) {
      retos = GetLastError();
      if (retos == ERROR_HANDLE_EOF) {
        break;
      }
      else {
        log_event("Unable to get %s file next stream stat, nb = %d, retos=%d\n", argv[2], nb, GetLastError());
      }
      FindClose(hfind);
      return 1;
    }

  }

  FindClose(hfind);

#else

#if (defined(MACOS) || defined(LINUX))

#if defined(LINUX)

    retos = lstat(argv[2], &st);
    if (retos == -1) {
      log_error(argv[0]);
      return 1;
    }

    if (S_ISLNK(st.st_mode)) {

      size = llistxattr(
		argv[2],
		NULL,
		0
		);

    }
    else {

#endif

      size = listxattr(
		argv[2],
		NULL,
		0
#ifdef MACOS
		,options
#endif
		);

#if defined(LINUX)
    }
#endif

    if (size == 0) {
      return 0;
    }

    if (size < 0) {
      log_error(argv[0]);
      return 1;
    }

    buf = (char *)malloc(size);

#if defined(LINUX)

    if (S_ISLNK(st.st_mode)) {

      size = llistxattr(
		argv[2],
		buf,
		size
		);

    }
    else {

#endif

      size = listxattr(
		argv[2],
		buf,
		size
#ifdef MACOS
		,options
#endif
		);

#if defined(LINUX)
    }
#endif

    if (size < 0) {
      log_error(argv[0]);
      return 1;
    }

    end = buf + size;

    while (buf < end) {

      printf("%s: \"", buf);
      fflush(stdout);

      if (!show_xattr(argv[2], buf)) {
        log_error(argv[0]);
        return 1;
      }

      printf("\"\n");
      buf += strlen(buf) + 1;

    }

#elif (defined(SUN))

  attrdirfd = attropen(argv[2], ".", O_RDONLY);
  if (attrdirfd == -1) {
    log_error(argv[0]);
    return 1;
  }

  dd = fdopendir(attrdirfd);
  if (dd == (DIR *)NULL) {
    close(attrdirfd);
    log_error(argv[0]);
    return 1;
  }
    
  while (p_dirent = readdir(dd)) {

    if ((strcmp(p_dirent->d_name, ".")  == 0) ||
        (strcmp(p_dirent->d_name, "..") == 0)) {
      continue;
    }
    
    printf("%s: \"", p_dirent->d_name);
    fflush(stdout);

    if (!show_xattr(argv[2], p_dirent->d_name)) {
      log_error(argv[0]);
      return 1;
    }

    printf("\"\n");
  
  }
    
  closedir(dd);
  close(attrdirfd);

#endif

#endif

    return 0;

  }
  else {
    log_event("Bad arguments\n");
    return 1;
  }

}
