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
- **Configuration**: XML-based configuration for routes, SSL, jwt permission roles, rate limiting and more.

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
  - nlohmann (included in the `third_party` folder).
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
        <!-- POST route for admin login, creates jwt token -->
        <Route method="POST" endpoint="/login" script="scripts/login.py" auth_role="admin" authenticator="true" args="json">
            <!-- Fixed window rate limiting, 200req/60s -->
            <RateLimit algorithm="fixed window" max_requests="200" window="60s">
                <!-- Rate limits clients based on the query string, if no query string is present, the server will return a 400 Bad Request -->
                <Key type="query" fallback="error"/>
            </RateLimit>
        </Route>

        <!-- POST route for staff endpoint, server forwards no arguments, requires 'editor' privileges -->
        <Route method="POST" endpoint="/staff" script="scripts/staff.php" protected="true" access_role="editor" args="none">
        <!-- Token bucket rate limiting, maximum of 10 tokens, refilling at a rate of 2 tokens/second -->
            <RateLimit algorithm="token bucket" capacity="10" refill_rate="2/s">
            <!-- Rate limits clients based off of the client component in the 'X-Forwarded-For' header, will fallback to IP by default if header is missing -->
                <Key type="xff"/>
            </RateLimit>
        </Route>

        <!-- GET route for endpoint /api, will forward the query string to the script, requires the default Role 'admin' to access -->
        <Route method="GET" endpoint="/api" script="scripts/api.py" protected="true" access_role="admin" args="query">
            <!-- Token bucket rate limiting, gives clients 960 token capacity, with a refill rate of 10 tokens/second -->
            <RateLimit algorithm="token bucket" capacity="960" refill_rate="10/s">
                <!-- Rate limits clients based off of the 'x-api-key' header, will return a 400 Bad Request if the header is missing -->
                <Key type="header" name="x-api-key" fallback="error"/>
            </RateLimit>
        </Route> 

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

    <!-- Global rate limit applies to all requests, can be disabled by setting disable="true" -->
    <Global disable="false">
    <!-- Fixed window rate limiting algorithm, allowing a total of 10000 requests per hour, results in 2 requests per second (10000/3600)  -->
        <RateLimit algorithm="fixed window" max_requests="10000" window="1hr">
            <Key type="xff" fallback="ip"/>
        </RateLimit>
    </Global>

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

- Endpoints can be set with the following attributes:
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

- Additionally Endpoints can be rate limited via either the `fixed window` or `token bucket` algorithm:
  ```xml
    <Route method="GET" endpoint="/my_api" script="scripts/my_api_script.py" protected="true" access_role="api_client" args="query">
        <RateLimit algorithm="token bucket" capacity="960" refill_rate="10/s">
            <Key type="header" name="x-api-key" fallback="error"/>
        </RateLimit>
    </Route>
    ```
- See the **Rate Limit Configuration** section below for more details. 


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

#### Troubleshooting Scripts

- Ensure that the script path in the config file is relative to the WebDirectory.
- Ensure executable permissions are set on the script for the user the server process is running under.
- The server expects the script to be executable as is.
    - The script must either be a compiled binary.
    - Or the script must contain a valid shebang line, e.g:
        ```c++
        # PHP
        #!/usr/bin/env php

        # Python
        #!/usr/bin/env python3

        # Perl
        #!/usr/bin/env perl

        # Ruby
        #!/usr/bin/env ruby

        # Node.js
        #!/usr/bin/env node
        ```

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

- Currently the server supports the **fixed window** and **token bucket** algorithm and identifies clients based off of an optional Key element, if not the present the server will identify clients based on their IP address. 
- RateLimit's can be set within each **Route** config, or under the **Global** config(seen below). 

#### Token Bucket

- The token bucket algorithm can be set with the following attributes:
  - **capacity**: The maximum number of tokens a client can have.
  - **refill_rate**: The rate at which tokens are replenished to the client. In tokens per unit time
    - **NOTE**: Do not add an integer to the unit of time.
    - **NOTE**: It is highly recommended to configure the rate in units of seconds (s), or ensure that the value of tokens squarely divides the time unit's seconds representation. This is because internally the server represents the rate as an integer in tok/s. Meaning, if one sets a rate of for example `90/m` (90 tokens per minute), this is then calculated to be `90 tok/60s = 1.5 tok/s => 1 tok/s (Integer)`.
    - For example:
    ```xml
    <Route method="POST" endpoint="/endpoint" script="scripts/endpoint_handler.rb" args="json">
            <!-- Recommended: results in a refill_rate of 2 tok/s -->
            <RateLimit algorithm="token bucket" capacity="200" refill_rate="2/s"/>

            <!-- Results in a refill_rate of 2 tok/s -->
            <RateLimit algorithm="token bucket" capacity="200" refill_rate="120/m"/>

            <!-- BAD: Will result in a refill_rate of 0 tok/s, since floor(30/60) = 0 -->
            <RateLimit algorithm="token bucket" capacity="200" refill_rate="30/m"/>

            <!-- BAD: Will fail to parse due to the '/2s' in '30/2s', instead choose '15/s' -->
            <RateLimit algorithm="token bucket" capacity="200" refill_rate="30/2s"/>
    </Route>
    ```

- **NOTE**: If any attribute is missing, the server will result to choosing the default values (see **Defaults** section below).

#### Fixed Window
   
- The fixed window algorithm can be set with the following attributes:
  - **max_requests**: The maximum number of requests served to the client within the window.
  - **window**: The time between max_requests resets.
  - For example:
  ```xml
    <Route method="POST" endpoint="/endpoint" script="scripts/endpoint_handler.rb" args="json">
        <!-- Note that only the first 'RateLimit' element will be parsed -->
        <RateLimit algorithm="fixed window" max_requests="1000" window="1hr"/>
        
        <RateLimit algorithm="fixed window" max_requests="1000" window="60m"/>

        <RateLimit algorithm="fixed window" max_requests="1000" window="3600s"/>
    </Route>
    ```
- **NOTE**: If any attribute is missing, the server will result to the default values (see **Defaults** section below).


#### Keys

- Rate limits can be further configured to choose the method of identifying the client, this is done through adding a Key element to the RateLimit, if no key is present, the server will default to the clients IP address.
- Keys can be set with the following attributes:
  - **type**: The method of identifying the client, supported options are:
    - **xff**: Identifies clients by the `X-Forwarded-For` header, specifically the client field within it.
    - **ip**: Identifies clients based on their IP address.
    - **query**: Identifies clients based on the query string present in the request uri.
    - **header**: Identifies clients based on a specific header. This option requires an additional attribute:
      - **name**: The name of the header used to identify the client, for example— `X-Api-Key`.
  - **fallback**: A fallback option for when the **type** isn't found or accessible. Supports two options:
    - **ip**: Fallback to identifying the client based on their IP address.
    - **error**: Return an error, currently the server simply returns a 400 Bad Request.

- The default values for both the **type** and **fallback** attributes are **"ip"**.

- For example: 
    ```xml
    <Route method="POST" endpoint="/endpoint" script="scripts/endpoint_handler.rb" args="json">
        <!-- Note that only the first 'RateLimit' element will be parsed, this is just an example -->
        <RateLimit algorithm="token bucket" capacity="1000" refill_rate="2/s">
            <!-- Identify the client based on the 'X-Api-Key' header, return a 400 if not present -->
            <Key type="header" name="x-api-key" fallback="error"/>
        </RateLimit>
        <RateLimit algorithm="fixed window" max_requests="1000" window="60m">
            <!-- Identify the client based on a query string, fallback to the client's IP address if no string present -->
            <Key type="query" fallback="ip"/>
        </RateLimit>
        <RateLimit algorithm="fixed window" max_requests="1000" window="3600s">
            <!-- Identify the client based on the client field within the 'X-Forwarded-For' header, fallback to ip if not present -->
            <Key type="xff" fallback="ip"/>
        </RateLimit>
    </Route>
    ```

- Rate Limits can also be set globally within the **Global** element.
- This setting applies rate limiting to all requests. By default, all requests will be rate limited using the **fixed window** algorithm, with a **window** of **60 seconds**, and a **max_requests** of **3000**. This is true regardless of whether the **Global** element is present.
- Global rate limiting can be disabled by setting the **disable** attribute to **"true"**:
    ```xml
    <Global disable="true"/>
    ```
- For example, to set global token bucket rate limiting, based off of the `X-Forwarded-For header`:
    ```xml
    <Global disable="false">
        <RateLimit algorithm="token bucket" capacity="1000" refill_rate="2/s">
            <Key type="xff" fallback="ip"/>
        </RateLimit>
    </Global>
    ```

 - Note that setting **disable="false"** is redundant, the server will automatically assume it is enabled if the disable attribute is not found.

#### Defaults

- The default algorithm for rate limiting globally—if the `RateLimit` element is empty, or the `algorithm` attribute is empty/unrecognized—is **fixed window**, with **max_requests** set to **3000** and **window** set to **60s**.
- For each algorithm below, if an element is unrecognized/not-present, the default value will be chosen. 

    ##### Fixed Window

    - **window**: 60s
    - **max_requests**: 5000

    ##### Token Bucket

    - **refill_rate**: 2 tokens/s
    -  **capacity**: 60 tokens
    
    ##### Keys

    - **type**: ip
    - **fallback**: ip
  
#### Time Units

- The configuration supports window and refill rates in time units of seconds, minutes, hours, and days (no decimals or fractions):
    - **seconds**: s, sec, secs
    - **minutes**: m , min, mins
    - **hours**: hr, hour, hours
    - **days**: d, day, days

- If the the time unit is unspecified or unrecognized, the server will assume seconds:
    ```xml
    <!-- 'minutes' not supported, will default to 12s, resulting in 500req/12s -->
    <RateLimit algorithm="fixed window" max_requests="500" window="12minutes"/>

    <!-- No unit provided, will default to 12s, resulting in 500req/12s -->
    <RateLimit max_requests="500" window="12"/>

    <!-- Correct units (m, min, mins), results in 500req/12min -->
    <RateLimit max_requests="500" window="12m"/>
    ```

- As mentioned earlier, when configuring the **refill_rate** attribute, it is **highly recommended** to use units of **seconds (s)**, or ensure that the value of tokens squarely divides the time unit's seconds representation. This is because internally the server represents the rate as an integer in tok/s. Meaning, if one sets a rate of—for example `90/m` (90 tokens per minute), this is then calculated to be `90 tok/60s = 1.5 tok/s => 1 tok/s (Integer)`. This may result in a loss of expected tokens, or in the worst case, 0 token refills: 

    ```xml
    <Route method="POST" endpoint="/misconfigured" script="scripts/my_script.py" args="query" protected="false">

        <!-- BAD: Will result in a refill_rate of 1 tok/s, NOT 1.5 tok/s -->
        <RateLimit algorithm="token bucket" capacity="200" refill_rate="90/m"/>

        <!-- BAD: Will result in a refill_rate of 0 tok/s, since floor(30/60) = 0 -->
        <RateLimit algorithm="token bucket" capacity="200" refill_rate="30/m"/>
    </Route>
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
<LogDirectory>/path/to/different/log</LogDirectory>
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
    - **DEBUG**: Slightly less verbose than trace, currently logs information useful for troubleshooting administrators.
    - **WARN**: Higher severity than error.
    - **FATAL**: This level is reserved for critical errors that result in the server exiting. The FATAL level will also write the stack trace, and the pid of the exiting process.
