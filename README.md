# Web-Server

## Overview

This is a custom-built CGI web server designed to serve dynamic and static content efficiently via configurable endpoint scripts. It currently powers my personal website, [lattadata.com](https://lattadata.com), which is hosted on my home server.

## Demo: Live Website
Visit my personal website:  [lattadata.com](https://lattadata.com)

**NOTE:** The project is a work in progress, there are many features/limitations I intend on developing/addressing, some of which are mentioned here in the README, others in the main.cpp file. The current implementation serves as prototype for a configurable, role permission based cgi-like web server.

## Features

- **GET, HEAD, POST, and OPTIONS Methods**: Supports retrieval of static resources and execution of server-side scripts.
- **Middleware Pipeline**: Modular middleware processing for tasks such as logging, error handling, authentication, and request parsing.
- **Dynamic Content Execution**: Executes scripts in response to POST and GET requests using POSIX system calls.
- **SSL Support**: Optional HTTPS support using OpenSSL.
- **Logging**: Detailed logging of request and response details, including latency and round-trip times.
- **Configuration**: XML-based configuration for routes, SSL, roles, and permissions.

## Requirements

- **Operating System**: Linux (required for syscall usage).
- **Build System**: [CMake.](https://cmake.org/download/)
    - Install via your package manager:
    ```bash
    sudo apt install cmake
    ```
- **Libraries**:
  - Asio (header-only, included in the `third_party` folder).
  - jwt-cpp (included in the `third_party` folder).
  - TinyXML2 (included in the `third_party` folder).
  - OpenSSL (likely included on most linux machines).

All dependencies(aside from cmake) are included in the `third_party` folder; no external installations are required.

## Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/IsaacLatta/Web-Server.git
   cd Web-Server
   ```
2. Create the build directory if it does not already exist:
   ```bash
   mkdir build && cd build
   ```
3. Configure the project:
   ```bash
   cmake ..
   ```
4. Build the project:
   ```bash
   make
   ```

## Usage

- The server expects a configuration file in xml format. The server expects this file path as its first command line argument:
   ```bash
   ./WebServer /path/to/server_configuration.xml
   ```

- The server_config.xml file defines the server's behavior. Key sections include:
    - WebDirectory: Specifies the root directory for static files.
    - LogDirectory: Optional directory used to house the log files.
    - Routes: Defines endpoints, HTTP methods, scripts, and permissions.
    - Roles: Specifies user roles and permissions.
    - SSL: Configures SSL settings, including certificate and key paths.
    - JWT: Configures the JWT setting used for generating secrets.
    - ErrorPages: Allows serving of predefined error pages for various error codes.
    - RateLimit: Allows rate limit settings based on the client ip address.

- Example xml configuration: 
   ```xml
    <?xml version="1.0" encoding="UTF-8"?>
   <ServerConfig>
    <!-- Specifies the directory containing static web files -->
    <WebDirectory>/var/www/html</WebDirectory>

    <!-- Specifies the number of threads the server will allocate -->
    <Threads>32</Threads>

    <!-- Optional directory where logs will be stored, will default to a log folder within the WebDirectory if none specified -->
    <LogDirectory>/etc/MyWebServer/log</LogDirectory>

    <!-- The name of the server as seen by clients -->
    <Host>MyWebServer</Host>

    <!-- Port on which the server will listen -->
    <Port>8080</Port>

    <!-- Role definitions -->
    <Roles>
        <!-- Owner role with access to all resources -->
        <Role title="owner">
            <Includes>*</Includes>
        </Role>
        <!-- Editor role with access to 'viewer' and 'user' permissions -->
        <Role title="editor">
            <Includes>viewer</Includes>
            <Includes>user</Includes>
        </Role>
    </Roles>

    <!-- SSL configuration for secure connections -->
    <SSL>
        <!-- Path to the SSL certificate -->
        <Certificate>/path/to/cert.pem</Certificate>
        <!-- Path to the SSL private key -->
        <PrivateKey>/path/to/key.pem</PrivateKey>
    </SSL>

    <!-- Route definitions -->
    <Routes>
        <!-- POST route for admin login, creates admin cookie -->
        <Route method="POST" endpoint="/login" script="scripts/login.py" auth_role="admin" authenticator="true" args="json"/>
        <!-- GET route with protection, server forwards no arguments, requires admin privilege to access -->
        <Route method="GET" endpoint="/admin" script="scripts/admin.php" args="none" protected="true" access_role="admin"/>
        <!-- GET route with protection, server will forward a query string from the client, requires admin privilege to access -->
        <Route method="GET" endpoint="/health" script="scripts/health.py" args="query" protected="true" access_role="admin"/>
        <!-- GET route with protection, server will forward a url form in the request body, requires admin privilege to access -->
        <Route method="GET" endpoint="/status" script="scripts/status.php" args="url" protected="true" access_role="admin"/>
        <!-- POST route with protection, server will forward json in the request body, requires admin privilege to access, will provide a JWT with the role set to owner -->
        <Route method="POST" endpoint="/protected_login" script="scripts/protected_login.py" args="json" protected="true" access_role="admin" authenticator="true" auth_role="owner"/>
    </Routes>

    <!-- ErrorPage definitions -->
    <ErrorPages>
        <!-- Serve 404.html on status 404 -->
	    <ErrorPage code="404" file="error_pages/404.html" />
	    <!-- Serve 500.html on status 500 -->
        <ErrorPage code="500" file="error_pages/500.html" />
    </ErrorPages>

    <JWT>
        <!-- Disabled JWT generation -->
    	<GenerateSecret enable="false">
		    <Length>128</Length> 
	    </GenerateSecret>
        <!-- Load the JWT secret from a file -->
	    <SecretFile>/path/to/jwt_secret.txt</SecretFile>
    </JWT>

    <RateLimit>
        <!-- IP based fixed window rate limiting, 200req/hr -->
        <Global disable="false" max_requests="200" window="1hr"/>
        
        <!-- Also valid, see Rate Limiting section for available options -->
        <!-- <Global max_requests="200" window="3600s"/> -->
        <!-- <Global disable="false" max_requests="4000" window="1d"/> -->
    </RateLimit>

</ServerConfig>

### Role Permissions

- Roles can be defined in the configuration file.
- The server uses a hierarchical role system.
    - Roles may include other roles to inherit privileges.
- The server uses the Set-Cookie header with jwt's to handle privileges.
- The role name is added the jwt body, the client can then authenticate via the "Cookie" header with the jwt set. The server then checks the jwt's role against that of the requested endpoint/resource

- The server includes the following default roles:
    - viewer
    - user: includes viewer
    - admin: includes user, viewer

- The * option can be used enable the role to include all roles.

### Endpoint Configuration

- Endpoints can be set with the following options:
    - **authenticator**
        - Generates a Cookie for the client based on the "auth_role" set in the config.
    - **protected**
        - Restricts access to the endpoint to only the "access_role" field, and roles that include this "access_role" field.
    - **access_role**
        - The role required in the JWT to access the endpoint.
    - **auth_role**
        - The role the client will receive in their JWT upon the script returning a success
    - **method**
        - The request method the endpoint is accessible from.
    - **script**
        - The script the server will execute in response to the request.
    - **args**
        - The desired argument format the client must provide to be forwarded to the script. If this argument type is not provided, the server will return status 400 or 419.
        - Options Include:
          - **json**: valid json in the request body.
          - **url**: valid url form in the request body.
          - **query**: valid query string, will extract from the ? to the end of the request uri, example:
            - '/search?q=awesome+cgi+web+server' gets forwarded to the script as 'q=awesome+cgi+web+server'
          - **body**: will forward the request body, the server only validates json or url format against the content-type header. If this header isnt present, the server will forward   the body as is.
          - **any** or '**\***': will forward any argument type. This option prioritizes forwarding a query string, if no query string is present, the server forwards the body in the exact same way as the **body** option.
          - **none**: no data gets forwarded to the script.

### Script Configuration

- The server expects the following of the script:
    - A valid HTTP status line, example:
    ```http
    HTTP/1.1 200 OK
    HTTP/1.1 403 Forbidden
    ```
    - Any additional headers may also be included.
    - The response header and response body must not exceed the BUFFER_SIZE(**262144 bytes**) macro defined in **MethodHandler.h**. 
        - This can be changed by simply tweaking the macro.
    - The server provides the arguments to the script over **stdin(fd 0)**, and expects the response over **stdout(fd 1)**.
    - The server provides the arguments in the format provided in the endpoint configuration.
- The server will only validate json and url-form content types.

### JWT Configuration

- The server has 3 configuration options for JWT secret creation. Note that this configuration must appear within the JWT xml block as seen in the example config.
    - **Secret String**: The actual secret used for signing the JWT tokens.
    ```xml
    <Secret>top-secret-string</Secret>
    ```
    - **Secret File**: The file to load the JWT secret from. This options expects the full file contents to be the secret, there is no support for .pem or .json etc.
    ```xml
    <SecretFile>/path/to/super/secret/file</SecretFile>
    ```
    - **Generate Secret**: Generate a random, unique JWT secret at runtime.
        - Must have the 'enable' option set to *"true"*.
        - The Length(in bytes) is optional, if unspecified the default length is 64 bytes.
    ```xml
    <GenerateSecret enable="true">
        <Length>128</Length>
    </GenerateSecret>
    ```

### Rate Limit Configuration

- Currently the server supports the fixed window algorithm and rate limits based on the clients ip address. There is no per endpoint rate limiting, which is why in all example configurations the settings are under **_"Global"_**.

- The Global RateLimit section has 3 attributes:
  - **max_requests**: The maximum number of requests the server will allow per ip in the given window length.
  - **window**: The time frame over which the requests counter will increment.
  - **disable**: An optional true/false field that allows Global ip rate limiting to be disabled entirely:
    ```xml
    <!-- Disabled rate limiting -->
    <Global disable="true"/>
    ```
        - Note that setting **disable="false"** is redundant, the server will automatically assume it is enabled if the disable attribute is not found.

- By default, the window length will be **60s**, with a maximum of **5000 requests**.
    - If an attribute is missing, the server will choose the default value for the missing attribute, regardless if the other is/isn't present:
    ```xml
    <!-- 'window' attribute is not present, server will set window to 60s, resulting in 300req/60s -->
    <Global max_requests="300"/>

    <!-- max_requests attribute is not present, server will set max_requests to 5000, resulting in 5000req/12min -->
    <Global window="12min"/>
    ```

    - If the time unit in the window attribute is not supported/specified (see unit section below), the server assumes seconds:
  ```xml
    <!-- 'minutes' not supported, will default to 12s, resulting in 500req/12s -->
    <Global max_requests="500" window="12minutes"/>

    <!-- No unit provided, will default to 12s, resulting in 500req/12s -->
    <Global max_requests="500" window="12"/>

    <!-- Correct units (m, min, mins), results in 500req/12min -->
    <Global max_request"500" window="12m"/>

- The configuration supports windowing time units of seconds, minutes, hours, and days (no decimals or fractions):
    - **seconds**: s, sec, secs
    - **minutes**: m , min, mins
    - **hours**: hr, hour, hours
    - **days**: d, day, days

- All rate limiting sections must appear in the **RateLimit** element within the **ServerConfig** element, under **Global**:
```xml
<ServerConfig>
    .
    .
    <RateLimit>
        <!-- 100 req/hr -->
        <Global max_requests="100" window="1hr"/>
    </RateLimit>
    .
    .
</ServerConfig>
```

### Server File Structure

- The running server's file structure is seen below:
```bash
├── public # Directory visible to all users.
│   
└── log # Holds the servers log files(prefixed by the name of the server, and date).
```
- Note that I have used a separate **scripts** and **error_pages** directory in my example, this is not required.

### Logging

- The server creates log files, prefixed by the name of the server and the date in the **log** directory, relative the root web directory:
```bash
/var/www/html/log/MyWebServer-YYYY-MM-DD.log
```
- This directory can be overridden by adding the LogDirectory field to the configuration file, this new path will not be relative to the **WebDirectory** path.
```xml
<LogDirectory>/path/to/different/log/</LogDirectory>
```

- The logs are formatted with readability in mind:
```bash
[2025-01-01 13:14] STATUS [Server] MyWebServer is running on [ubuntu 203.0.113.84:8080] pid=60630
[2024-12-31 15:35] INFO [client 198.51.100.45] "GET / HTTP/1.1" Mozilla/5.0 on X11; Linux x86_64 Chrome/131.0.0.0 [Latency: 21 ms RTT: 2 ms Size: 2 KB] HTTP/1.1 200 OK
[2024-12-24 15:15] ERROR [client 192.0.2.13] ""  [Latency: 3 ms RTT: 84654619 ms] HTTP/1.1 500 Internal Server Error
[2025-01-01 15:24] INFO [client 172.16.254.1] "GET / HTTP/1.1" Mozilla/5.0 on Macintosh; Intel Mac OS X 10_15_7 Safari/605.1.15 [Latency: 0 ms RTT: 0 ms Size: 2 KB] HTTP/1.1 200 OK
[2024-12-21 19:23] ERROR [client 172.20.10.1] "GET /admin HTTP/1.1" Mozilla/5.0 on X11; Linux x86_64 Chrome/131.0.0.0  [Latency: 172 ms RTT: 5 ms] HTTP/1.1 401 Unauthorized
```

- Some additional log levels are also supported, all of which append the (file:line function):
    - **TRACE**: The most verbose level.
    - **DEBUG**: Slightly less verbose than trace, currently logs information useful troubleshooting administrators.
    - **WARN**: Higher severity than error.
    - **FATAL**: This level is reserved for critical errors that result in the server exiting. The FATAL level will also write the stack trace, and the pid of the exiting server process.

## To Do

1. POST Request.
    - Add support for resource uploads, as well as task submission, e.g.) streaming.
2. PUT Request.
    - Allow upload of resource.
3. OPTIONS Request.
    - Allow preflight for resource creation (use with POST).
4. CONNECT Support.
    - For tunneling (HTTPS).
5. Default endpoints.
    - Endpoints for health or status metrics, e.g.) /health, /status.
6. Add rate limiting, especially for POST.
7. Add example files config, static gifs, scripts etc for example usage.
8. Add documentation to the code base.


