# nanorest
A very small web server to provide a restful interface for a workstation.

## Server
Run from the command prompt.
```
> nanorestfile.[exe|osx|linux] [port] [inaddr_any]
```
Defaults are port 2020 and inaddr_loopback. The latter means that it is only accessible to browsers on the same machine. By setting inaddr_any the server can be accessible from anywhere on the local network.

Run it from your home development directory so that you can access any of your projects.

## Client

The `index.html` file shows examples of all the supported functions available via XMLHttpRequest. For files:

* **GET** retrieves the content of the file
* **PUT** creates or replaces an existing file with contents from the body of the request
* **DELETE** will delete the file reference in the uri.

Directories end in /, as in http://localhost/dir1/dir2/. The functions available are:

* **GET** Return a list of files in the specified uri. The list includes the full path and directories end in /
* **POST** will create the directory in the URI if it does not already exist.
* **DELETE** will remove the directory if it is empty.

In all cases check the return code. A 200 is returned if all is well.
