# Minimum Thread Commissioner

This directory includes an example of building a minimum Thread Commissioner with the OT Commissioner library. The minimum Thread Commissioner petitions to given Border Router, enables MeshCoP for all joiners and commissions joiners.

## Build

See [BUILDING.md](./BUILDING.md) for building this mini Commissioner program. You can find details about the Border Router (`ot-daemon`) and simulation devices in this [codelab](https://openthread.io/codelabs/openthread-simulation-posix).

## Run the Commissioner

### Start the BR (ot-daemon)

See the [codelab](https://openthread.io/codelabs/openthread-simulation-posix).

### Start the Mini Commissioner

After a successful build, we should get a `mini_commissioner` binary which can be started with four arguments:

```shell
./mini_commissioner ::1 49191 00112233445566778899aabbccddeeff ABCDEF
```

You can get the usage by starting `mini_commissioner` with no arguments:

```shell
./minim_commissioner
usage:
    mini_commissioner <br-addr> <br-port> <pskc-hex> <pskd>
```

> Note: the WiFi/ethernet interface address of the BR should be used but not the Thread interface address.

If everything go smooth, we will get outputs like below:

```shell
./mini_commissioner ::1 49191 ca117352886a861cce8a91021e65dd1c ABCDEF
===================================================
[Border Router address] : ::1
[Border Router port]    : 49191
[PSKc]                  : ca117352886a861cce8a91021e65dd1c
[PSKd]                  : ABCDEF
===================================================

===================================================
type CRTL + C to quit!
===================================================

petitioning to [::1]:49191
the commissioner is active: true
enabling MeshCoP for all joiners
waiting for joiners
```

> Note: you are free to quit at any time with `CTRL+C`.

### Start the joiner

See the [codelab](https://openthread.io/codelabs/openthread-simulation-posix).

if everything go smooth, we will get output like below for two times of joining:

```shell
joiner "5ab1f2745b625c90" is requesting join the Thread network
joiner "5ab1f2745b625c90" is connected: OK
joiner "5ab1f2745b625c90" is commissioned
[Vendor Name]          : OPENTHREAD
[Vendor Model]         : NRF52840
[Vendor SW Version]    : 20191113-01632-g
[Vendor Stack Version] : f4ce36000010
[Provisioning URL]     :
[Vendor Data]          :

joiner "5ab1f2745b625c90" is requesting join the Thread network
joiner "5ab1f2745b625c90" is connected: OK
joiner "5ab1f2745b625c90" is commissioned
[Vendor Name]          : OPENTHREAD
[Vendor Model]         : NRF52840
[Vendor SW Version]    : 20191113-01632-g
[Vendor Stack Version] : f4ce36000010
[Provisioning URL]     :
[Vendor Data]          :

```
