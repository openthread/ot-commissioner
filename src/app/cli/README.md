# OT Commissioner CLI

Use the OT Commissioner CLI to configure and manage OT Commissioner.

## Commands

Type `help` to get a command list:

```shell
> help
active
announce
bbrdataset
borderagent
br
commdataset
domain
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

To cancel a command, send signal `Interrupt` to OT Commissioner, which is `CTRL + C` for a Linux machine.

### Multi-network syntax

The commands below are able to work with multiple Thread networks simultaneously:

```shell
start
stop
active
sessionid
bbrdataset get
commdataset get
opdataset get active
opdataset get pending
opdataset set securitypolicy
br list
br delete
br scan
domain list
network list
token request
```

Multi-network syntax uses `--nwk <network-alias>` or `--dom <domain-alias>` keys to identify set of target networks for a command. Another provided possibility is network selection (see `network select` below). Explicit aliases on the command line override network selection for the command.

Multi-network commands execute as parallel single-network commands. Nevertheless the whole commissioner command executes synchronously.

The supported network aliases (in order of resolution) are:

- `all` - attempt command execution on all known networks in the registry (see below).
- `this` - command affects the selected network only.
- `other` - command affects all but selected network.
- network extended PAN ID (as hexadecimal string).
- network name.
- network PAN ID. Special network aliases `all`, `this`, and `other` are mutually exclusive and incompatible with other aliaces (i. e. only one such alias alone is allowed). It is possible to provide multiple explicit network aliases (XPANs, names, or PAN IDs) on the command line after the single `--nwk` key:

```shell
start --nwk all
active --nwk this
sessionid --nwk other
stop --nwk DEAD00BEEF00CAFE ThreadNet1 0xFACE
```

The supported domain aliaces are (in order of resolution):

- `this` - domain of the selected network.
- domain name. Only one domain alias is allowed per command: no command can span multiple domains:

```shell
start --dom this
stop --dom ThreadDomain
```

Using `--nwk` and `--dom` keys simultaneously is unsupported.

### Registry

Commissioner has internal registry of the known Thread networks. This registry with security materials storage (see below) allows accessing Thread networks in a user-friendly manner with multi-network syntax.

Persistent registry has form of a JSON file stored as defined by `-r|--registry <registry-file-name` commissioner startup parameter. If no such parameter provided, the registry will be stored in memory, so won't persist.

Registry manipulation commands are:

```shell
br add
br delete
br list
network list
network select
network identify
domain list
```

Note that `network select` command affects multi-network commands execution.

### Network selection concept

In multi-network environment it may be useful to manage multiple network tasks in duration of a single session. To simplify command execution a particular network may be selected to deal with by default. Selection would allow more concise command syntax to be used. For example, when selected with `network select` the network can be started with simple `start` command where the missing specification of border router address and port can be augmented automatically using information from Registry.

To make a selection the network must be specified by a network alias string, where the alias may be:

- an extended PAN ID in hexadecimal notation
- a network name
- a PAN ID in hexadecimal notation

Examples:

```shell
network select DEAD00BEEF00CAFE
network select Network-1
network select FACE
```

**Note**: hexadecimal notation may or may not include `0x` prefix. The following are equivalent:

```shell
network select DEAD00BEEF00CAFE
network select 0XDEAD00BEEF00CAFE
network select dead00beef00cafe
network select 0xdead00beef00cafe
```

Now a brief syntax of multi-network aware commands may be used effectively against the selected network as if it is a sole one connected at the time. All the other existing connection to Thread networks will remain unaffected.

Examples:

```shell
network select DEAD00BEEF00CAFE
start
opdataset get active
stop
```

To drop the current network selection a special alias `none` must be used:

```shell
network select none
```

From this moment no network is implied by default, and explicit network/domain-wise syntax must be used until the next selection.

### Deleting entries from the registry

`br delete` command is the only command capable of deleting entries from the registry. While command explicitly deletes only border router entries, ot has recursive behavior. That is the deleting the last border router from the network will results in network deletion, and deleting last network from domain deletes the domain too.

Note that deleting the last border router entity in the selected network is prohibited add will result in error message.

## Security materials storage

The storage root is passed to commissioner via configuration JSON object optional parameter `ThreadSMRoot`. If the parameter is not found in configuration, application falls back to environment variable `THREAD_SM_ROOT`. The environment variable can be easily configured using a shell script per scenario. If environment variable is also not available, security materials from configuration will be used by default.

Domain-based security materials:

```
  - certificate:  $THREAD_SM_ROOT/dom/$domain_id/cert.pem
  - private key:  $THREAD_SM_ROOT/dom/$domain_id/priv.pem
  - trust anchor: $THREAD_SM_ROOT/dom/$domain_id/ca.pem
  - COM_TOK:      $THREAD_SM_ROOT/dom/$domain_id/tok.cbor
```

Network-based:

```
  - certificate:  $THREAD_SM_ROOT/nwk/$network_id/cert.pem
  - private key:  $THREAD_SM_ROOT/nwk/$network_id/priv.pem
  - trust anchor: $THREAD_SM_ROOT/dom/$network_id/ca.pem
  - COM_TOK:      $THREAD_SM_ROOT/nwk/$network_id/tok.cbor
  - PSKc:         $THREAD_SM_ROOT/nwk/$network_id/pskc.txt
```

Example:

```
$THREAD_SM_ROOT
               \_ dom
               |      \_ domain1
               |      |         \_ cert.pem
               |      |         |_ priv.pem
               |      |         |_ ca.pem
               |      |         |_ tok.cbor
               |      \_ domain2
               |                \_ cert.pem
               |                |_ priv.pem
               |                |_ ca.pem
               |                |_ tok.cbor
               |_ nwk
                     \_ AC69B4FE5F428433
                     |                  \_ pskc.txt
                     |_ EB7EA78599265FB7
                     |                  \_ pskc.txt
                     |_ DEAD00BEEF00CAFE
                                        \_ cert.pem
                                        |_ priv.pem
                                        |_ ca.pem
                                        |_ tok.cbor
```

**Note**: it is user's duty to take all possible care about completeness and consistency of the stored security materials and the structure of the storage.

#### Looking up in SM storage

A configuration object must include DTLS configuration that corresponds to the network the CommissionerApp instance to deal with. Therefore, some lookup phase must precede the instance initialization.

**Note**: The logic is a part of `start` command pre-processing and all the other commands do not use this.

- Configuration object is cloned from default configuration.
- If target network belongs to a domain:
  - the domain-based path pattern `$THREAD_SM_ROOT/dom/$domain_id/` must be constructed and tested for presence
    - if folder exists
      - the `cert.pem`, `priv.pem`, `ca.pem` and `tok.cbor` files must be tested as well
      - if any of the file test fails (except `tok.cbor` whichis optional)
        - error reported for the files
      - any newly confirmed SM path is set to the configuration object
- If network is standalone one or still no files found above
  - the network-based path pattern `$THREAD_SM_ROOT/nwk/$network_id/` must be constructed and tested for presence
    - if folder exists
      - if network Connection Mode expects certificate-based connection
        - the `cert.pem`, `priv.pem`, `ca.pem` and `tok.cbor` files must be tested as well
      - else the `pskc.txt` file must be tested for presence
      - if any of the file test fails (again with exception of the `tok.cbor`)
        - error reported for the file
      - any newly confirmed SM path is set to the configuration object

#### When SM storage root is not configured

If SM storage root is not configured by any means, it must not change DTLS options inherited from the configuration JSON object.

**Example** use case is where only a single **COM_TOK** is defined by a configuration JSON file, e.g. because there’s only one Thread Domain. In this case the user could opt to not define a **THREAD_SM_ROOT** structure and only specify all security material by the JSON config.

```
"PrivateKeyFile" : "/usr/local/etc/commissioner/credentials/private-key.pem",
"CertificateFile" : "/usr/local/etc/commissioner/credentials/certificate.pem",
"TrustAnchorFile" : "/usr/local/etc/commissioner/credentials/trust-anchor.pem",
"ComTokFile”: "/usr/local/etc/commissioner/credentials/my-com-tok.cbor"
```

### Export/import syntax

The commissioner supports exporting command execution results in JSON form by appending `--export <JSON-file-name>` key to the following commands:

```shell
bbrdataset get
br scan
commdataset get
opdataset get active
optdataset get pending

```

By appending `--import <JSON-file-name>` parameter it is possible to put the referenced JSON as the last parameter of the following commands:

```shell
opdataset set active
opdataset set pending
```

There is no requirement for JSON to be formatted as single string for importing.

Import/export syntax is compatible with multi-network syntax.

### Help

List all commands.

### Commissioner token

Per the Thread 1.2 specification, `COM_TOK` is required for signing requests. **If running in CCM mode or accessing CCM network in multi-network style, the Commissioner must request and set `COM_TOK` before connecting to a Thread network.**

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
> token set ./COM_TOK.hex
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

```shell
### Petition with border agent belonging to the selected network; address, port, and connection mode selected by application according to data in the registry
> start --nwk this
[done]
```

### Active

Upon success, the Commissioner periodically sends keep alive messages in background. The Commissioner now is in the active state:

```shell
> active
true
[done]
> active --nwk DEAD00BEEF00CAFE
[
    "DEAD00BEEF00CAFE" : true
]
[done]
>
```

### Stop

Before exiting, use the command `stop` to gracefully leave the Thread network:

```shell
> stop
[done]
> stop --dom this
[
    "DEAD00BEEF00CAFE" : done,
    "AAAA00BBBB00CCCC" : done
]
[done]
```

### Exit

The command `exit` exits the CLI session.

### Commissioner Session ID

`sessionid` returns the Commissioner Session ID.

### Border Agent

The command `borderagent` provides access to Border Agent information and scans for Border Agent services.

```shell
> help borderagent
usage:
borderagent discover [<timeout-in-milliseconds>] [<specified-network-interface>]
borderagent get locator
[done]
>
```

Discover Border Agent services:

```shell
> borderagent discover
Addr=172.23.57.126
Port=49152
Discriminator=76db7e6dcb420b1c
ThreadVersion=1.2.0
State.ConnectionMode=1(PSKc)
State.ThreadIfStatus=2(active)
State.Availability=1(high)
State.BbrIsActive=1
State.BbrIsPrimary=1
NetworkName=OpenThread
ExtendedPanId=0xdead00beef00cafe
[done]
```

Get Border Agent locator:

```shell
> borderagent get locator
0x5000
[done]
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

- to enable a new MeshCoP joiner:

  ```shell
  ### The second argument of the joiner command is the type of joiner.
  ### It can be only ae, nmkp, or meshcop.
  > joiner enable meshcop 0x0123456789abcdef PSKD001
  [done]
  >
  ```

- or provide a provisioning URL:

  ```shell
  > joiner enable meshcop 0x0123456789abcdef PSKD001 https://google.com
  [done]
  >
  ```

- or enable all MeshCoP joiners:

  ```shell
  > joiner enableall meshcop PSKD001
  [done]
  >
  ```

- to enable a new CCM AE joiner:

  ```shell
  > joiner enable ae 0x0123456789abcdef
  [done]
  >
  ```

- to enable all CCM AE joiners:

  ```shell
  > joiner enableall ae
  [done]
  >
  ```

- to enable a new CCM NMKP joiner:

  ```shell
  > joiner enable nmkp 0x0123456789abcdef
  [done]
  >
  ```

- to enable all CCM NMKP joiners:

  ```shell
  > joiner enableall nmkp
  [done]
  >
  ```

- to get joiner UDP ports:

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

- to set joiner UDP ports:

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
network sync
[done]
>
```

#### Sync network data

This command pushes local Commissioner Dataset to the Thread Network and pulls down Active / Pending Dataset and BBR Dataset (if in CCM mode) to local.

```shell
> network sync
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

### Reenroll

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

## Border router registry entries manipulation

```shell
> help br
usage:
br scan [--nwk <network-alias-list> | --dom <domain-name>] [--export <json-file-path>] [--timeout <ms>]
br add <json-file-path>
br list [--nwk <network-alias-list> | --dom <domain-name>]
br delete (<br-record-id> | --nwk <network-alias-list> | --dom <domain-name>)
[done]
>

### (Initial) discovery of border routers (default timeout is 10000 ms).
### The command provides data on border routers and Thread networks and domains they belong to.
> br scan --timeout 2000 --export border-routers.json
[done]

### Re-discover border routers for the specific network
> br scan --nwk ThreadNetwork --export brs-ThreadNetwork.json
[done]

### Insert or update (if present) border routers data in the registry (from discovery results)
> br add border-routers.json
[done]

### List known border routers
> br list
[
    {
        "addr": "fd01:1234:5678:0:85bd:7906:c79c:81b8",
        "bbr_port": 48936,
        "bbr_seq_number": 60,
        "id": 3,
        "nwk_ref": 3,
        "port": 49191,
        "service_name": "net1._meshcop._udp.local.",
        "state_bitmap": 295,
        "thread_version": "1.2.0"
    },
    {
        "addr": "fd01:1234:5678:0:db87:9c61:63d4:5d80",
        "bbr_port": 49048,
        "bbr_seq_number": 115,
        "id": 4,
        "nwk_ref": 4,
        "port": 49191,
        "service_name": "net3._meshcop._udp.local.",
        "state_bitmap": 295,
        "thread_version": "1.2.0"
    },
    {
        "addr": "fd01:1234:5678:0:c5b2:7f6:1f43:3ff4",
        "bbr_port": 48968,
        "bbr_seq_number": 126,
        "id": 5,
        "nwk_ref": 5,
        "port": 49191,
        "service_name": "net2._meshcop._udp.local.",
        "state_bitmap": 295,
        "thread_version": "1.2.0"
    }
]
[done]

### Delete border router entry by registry identifier
> br delete 4
[done]

### Delete all border routers in the network 'net1' (which is assumed to be selected network)
> br delete --nwk net1
IO_ERROR: Can't delete all border routers from the current network
[failed]
>
```

## Network registry entries manipulation

```shell
> help network
usage:
network list [--nwk <network-alias-list> | --dom <domain-name>]
network select <xpan>|none
network identify
[done]

### List networks in the domain DomTCE1
> network list --dom DomTCE1
[
    {
        "ccm": 1,
        "channel": 0,
        "dom_ref": 2,
        "id": 3,
        "mlp": "",
        "name": "net1",
        "pan": "",
        "xpan": "01DEAD00BEEF0001"
    },
    {
        "ccm": 1,
        "channel": 0,
        "dom_ref": 2,
        "id": 5,
        "mlp": "",
        "name": "net2",
        "pan": "",
        "xpan": "02DEAD00BEEF0002"
    }
]
[done]
>

### Set network 'net2' as current
> network select net2
[done]

### Show selected network
> network identify
{
    "02DEAD00BEEF0002": "DomTCE1/net2"
}
[done]
>

### Explicitly discared network selection
> network select none
done
[done]
> network identify
none
[done]
```

## Domain registry entries manipulations

```shell
> help domain
usage:
domain list [--dom <domain-name>]
[done]

### List all known domains in the registry
> domain list
[
    {
        "id": 2,
        "name": "DomTCE1"
    },
    {
        "id": 3,
        "name": "DomTCE2"
    }
]
[done]

### Show domain of the selected network
> network select net2
[done]
> domain list --dom this
[
    {
        "id": 2,
        "name": "DomTCE1"
    }
]
[done]

```
