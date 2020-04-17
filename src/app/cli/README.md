# OT Commissioner CLI

Use the OT Commissioner CLI to configure and manage OT Commissioner.

## Build

```shell
ot-commissioner $ ./script/bootstrap.sh
ot-commissioner $ mkdir build && cd build
build $ cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
build $ ninja -j16
```

## Configuration

OT Commissioner requires a JSON configuration file. There are two configuration templates to choose from:

* Commercial Commissioning Mode (CCM) — [src/app/etc/commissioner/ccm-config.json](../etc/commissioner/ccm-config.json)
* Non-CCM — [src/app/etc/commissioner/non-ccm-config.json](../etc/commissioner/non-ccm-config.json)

Make a copy of your desired configuration file and modify it as needed for your system.

For the CCM configuration file, you need to change `DomainName` to the Thread domain you want to connect to. You also need to change `PrivateKeyFile`, `CertificateFile` and `TrustAnchorFile` to credentials you are going to use (default credentials are available in [src/app/etc/commissioner/credentials](../etc/commissioner/credentials) for testing).

For the non-CCM configuration file, you need to set `PSKc`.

## Start

When started, the CLI enters an interactive mode and waits for user commands.

```shell
## Example of starting a non-CCM Commissioner.
build $ ./src/app/cli/commissioner-cli ../src/app/etc/commissioner/non-ccm-config.json
>
```

## Commands

Type `help` to get a command list:

```shell
> help
active
announce
bbrdataset
borderagent
commdataset
domainreset
energy
exit
help
joiner
migrate
mlr
network
opdataset
panid
reenroll
sessionid
start
stop
token

type 'help <command>' for help of specific command.
[done]
>
```

All commands are synchronous, which means the command will not return until success, an error occurs, or a time out.

To abort a command, send signal `Interrupt` to OT Commissioner, which is `CTRL + C` for a Linux machine.

### Help

List all commands.

### Commissioner token

Per the Thread 1.2 specification, `COM_TOK` is required for signing requests. **If running in CCM mode, the Commissioner must request and set `COM_TOK` before connecting to a Thread network.**

#### Request `COM_TOK`

```shell
### The registrar listening at [fdaa:bb::de6]::5684
> token request fdaa:bb::de6 5684
[done]
>
```

#### Print `COM_TOK`

```shell
### Print the requested Token (COSE_SIGN1 signed) as a hex string.
> token print
d28443a10126a058aea40366546872656164047818323031392d31322d30315431303a33383a34392e3336315a0178280414efbfbd080b387defbfbdefbfbd5b3a59efbfbdefbfbd100befbfbd63efbfbdefbfbdefbfbd4c08a101a5024f4f542d636f6d6d697373696f6e6572225820c5c910c93a20868fc8504d370f1b26fa9759f67983bb78863eda9acbea5124f601022158202030855be846910b3ecb15caf8572d3eb565bdc654a15efaf6edebaa8b9e160820015840e491bbae36d7f4209190677878cb959fb80b6406ca7cf65039cc09812918a1f65934cf5f4b0f5d1eed97b994bd3e1ceca627f765cdc1bc2825c28039cd746032
[done]
>
```

#### Force set `COM_TOK`

```shell
## The trustAnchor is used to validate signature in the COM_TOK.
> token set ./COM_TOK.hex ./commissioner/trustAnchor.pem
[done]
>
```

### Start

To start petitioning as the active Commissioner of a Thread network:

```shell
### Petition with Border Agent at [::]:49191
> start :: 49191
[done]
>
```

### Active

Upon success, the Commissioner periodically sends keep alive messages in background. The Commissioner now is in the active state:

```shell
> active
true
[done]
>
```

### Stop

Before exiting, use the command `stop` to gracefully leave the Thread network:

```shell
> stop
[done]
>
```

### Exit

The command `exit` exits the CLI session.

### Commissioner Session ID

`sessionid` returns the Commissioner Session ID.

### Border Agent

The command `borderagent` provides access to Border Agent information:

```shell
> help borderagent
usage:
borderagent get locator
borderagent get meshlocaladdr
[done]
>
```

### Backbone Router dataset

In a Thread CCM network, the TRI and Registrar address is managed as parameters of the Backbone Router (BBR) dataset:

```shell
> help bbrdataset
usage:
bbrdataset get trihostname
bbrdataset set trihostname <TRI-hostname>
bbrdataset get reghostname
bbrdataset set reghostname <registrar-hostname>
bbrdataset get regaddr
bbrdataset get
bbrdataset set '<bbr-dataset-in-json-string>'
[done]
>

### The default TRI and registrar hostname is [fdaa:bb::de6].
> bbr get trihostname
[fdaa:bb::de6]
[done]
>

> bbr get reghostname
[fdaa:bb::de6]
[done]
>

> bbr get regaddr
fdaa:bb::de6
[done]
>

### The TRI or registrar hostname must contains a global ipv6 address
### enclosed by brackets.
> bbr set trihostname [fdaa:bb::de8]
[done]
>

> bbr set reghostname [fdaa:bb::de8]
[done]
>
```

### Steering dataset

The Commissioner steers joining of new devices by `Steering Data` in the Commissioner dataset.

```shell
> help joiner
usage:
joiner enable (meshcop|ae|nmkp) <joiner-eui64> [<joiner-password>] [<provisioning-url>]
joiner enableall (meshcop|ae|nmkp) [<joiner-password>] [<provisioning-url>]
joiner disable (meshcop|ae|nmkp) <joiner-eui64>
joiner disableall (meshcop|ae|nmkp)
joiner getport (meshcop|ae|nmkp)
joiner setport (meshcop|ae|nmkp) <joiner-udp-port>
[done]
>
```

* to enable a new MeshCoP joiner:

    ```shell
    ### The second argument of the joiner command is the type of joiner.
    ### It can be only ae, nmkp, or meshcop.
    > joiner enable meshcop 0x0123456789abcdef PSKD1
    [done]
    >
    ```

* or provide a provisioning URL:

    ```shell
    > joiner enable meshcop 0x0123456789abcdef PSKD1 https://google.com
    [done]
    >
    ```

* or enable all MeshCoP joiners:

    ```shell
    > joiner enableall meshcop PSKD1
    [done]
    >
    ```

* to enable a new CCM AE joiner:

    ```shell
    > joiner enable ae 0x0123456789abcdef
    [done]
    >
    ```

* to enable all CCM AE joiners:

    ```shell
    > joiner enableall ae
    [done]
    >
    ```

* to enable a new CCM NMKP joiner:

    ```shell
    > joiner enable nmkp 0x0123456789abcdef
    [done]
    >
    ```

* to enable all CCM NMKP joiners:

    ```shell
    > joiner enableall nmkp
    [done]
    >
    ```

* to get joiner UDP ports:

    ```shell
    > joiner getport meshcop
    1000
    [done]
    > joiner getport ae
    1001
    [done]
    > joiner getport nmkp
    1002
    [done]
    >
    ```

* to set joiner UDP ports:

    ```shell
    > joiner setport meshcop 1000
    [done]
    > joiner setport ae 1001
    [done]
    > joiner setport nmkp 1002
    [done]
    >
    ```

### Operational dataset

A command `opdataset` is provided to get or set active or pending operational datasets:

```shell
> help opdataset
usage:
opdataset get
opdataset get activetimestamp
opdataset get channel
opdataset set channel <page> <channel> <delay-in-milliseconds>
opdataset get channelmask
opdataset set channelmask (<page> <channel-mask>)...
opdataset get xpanid
opdataset set xpanid <extended-pan-id>
opdataset get meshlocalprefix
opdataset set meshlocalprefix <prefix> <delay-in-milliseconds>
opdataset get networkmasterkey
opdataset set networkmasterkey <network-master-key> <delay-in-milliseconds>
opdataset get networkname
opdataset set networkname <network-name>
opdataset get panid
opdataset set panid <panid> <delay-in-milliseconds>
opdataset get pskc
opdataset set pskc <PSKc>
opdataset get securitypolicy
opdataset set securitypolicy <security-policy>
opdataset get active
opdataset set active '<active-dataset-in-json-string>'
opdataset get pending
opdataset set pending '<pending-dataset-in-json-string>'
[done]
>
```

The `opdataset` always operates on the active operational dataset except for **setting parameters that can not be directly set in an active operational dataset**. Those parameters are:

- channel
- meshlocalprefix
- networkmasterkey
- panid

OT Commissioner sends a `MGMT_PENDING_SET.req` message to set those parameters.

### Network data

```shell
### Network data is always stored in JSON format.
> help network
usage:
network save <network-data-file>
network pull
[done]
>
```

#### Pull network data

This command pulls down all Thread network data (active or pending dataset, commissioner dataset, BBR dataset) to local.

```shell
> network pull
[done]
>
```

This command is automatically executed when successfully connected to a Border Agent.

#### Save network data

This command saves all Thread network data to a JSON file.

```shell
> network save ./thread-test-network-data.json
[done]
>
```

### Announce

Instruct one or more devices to announce its Operational Dataset:

```shell
> help announce
usage:
announce <channel-mask> <count> <period> <dst-addr>
[done]
>
```

### Energy scan

`energy scan` instructs one or more devices to perform energy scanning. Use `energy report` to get the results.

```shell
### 'dst-addr' can be both unicast & multicast address.
> help energy
usage:
energy scan <channel-mask> <count> <period> <scan-duration> <dst-addr>
energy report [<dst-addr>]
[done]
>
```

### Detect PAN ID conflicts

`panid query` requests one or more devices to start scanning for PAN ID conflicts. Use `panid conflict` to get the results.

```shell
### 'dst-addr' can be either a unicast or multicast address.
> help panid
usage:
panid query <channel-mask> <panid> <dst-addr>
panid conflict <panid>
[done]
>
```

### Reenoll

To command a Thread device to perform MGMT reenrollment, use the `reenroll` command:

```shell
> reenroll fdde:ad00:beef:0:0:ff:fe00:fc00
[done]
>
```

### Domain reset

 To command a Thread device to perform MGMT domain reset, use the `domainreset` command:

```shell
> domainreset fdde:ad00:beef:0:0:ff:fe00:fc00
[done]
>
```

### Network migration

To command a Thread device to perform MGMT network migration, use the `migrate` command:

```shell
> migrate fdde:ad00:beef:0:0:ff:fe00:fc00 test-network-2
[done]
>
```

### Multicast listener registration

```shell
> help mlr
usage:
mlr <multicast-addr> <timeout-in-seconds>
[done]
>
```

## Datasets in JSON

There are advanced commands which accept or return datasets encoded in a JSON string.

The key of each JSON field in a dataset is produced by removing the prefix `m` of the case-sensitive dataset struct field name defined in [include/commissioner/network_data.hpp](../../../include/commissioner/network_data.hpp). Review those dataset structures before crafting a JSON object by yourself.

Each advanced command always sends associative `MGMT_*_(GET|SET).req` requests.

### Read/Write BBR dataset

- `bbrdataset get` -- get the whole BBR dataset as a JSON string;
- `bbrdataset set '<bbr-dataset-in-json-string>'`

### Read/Write Commissioner dataset

- `commdataset get` -- get the whole commissioner dataset as a JSON string;
- `commdataset set '<commissioner-dataset-in-json-string>'`

### Read/Write operational dataset

- `opdataset get active` -- get the whole active operational dataset as a JSON string;
- `opdataset set active '<active-dataset-in-json-string>'`
- `opdataset get pending` -- get the whole pending operational dataset as a JSON string;
- `opdataset set pending '<pending-dataset-in-json-string>'`
