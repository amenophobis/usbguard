# USBGuard

[![Build Status](https://travis-ci.org/dkopecek/usbguard.svg?branch=master)](https://travis-ci.org/dkopecek/usbguard)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/7605/badge.svg)](https://scan.coverity.com/projects/dkopecek-usbguard)

The USBGuard software framework helps to protect your computer against rogue USB devices (a.k.a. [BadUSB](https://srlabs.de/badusb/)) by implementing basic whitelisting/blacklisting capabilities based on USB device attributes.

**WARNING**: The 0.x releases are not production ready packages. They serve for tech-preview and user feedback purposes only. Please share your feedback or request a feature in the Github issue trackers for each project:

 * [Report a bug or request a feature in **usbguard**](https://github.com/dkopecek/usbguard/issues/new)
 * [Report a bug or request a feature in **usbguard-applet-qt**](https://github.com/dkopecek/usbguard-applet-qt/issues/new)

## Use Cases

### Use case #1: USB device whitelisting
A  desktop user usually has a quite stable set of USB devices he uses. The set consists of internally and externally connected devices. From a security point of view, it’s good to limit the usage of the computer’s USB ports to these devices only with some temporary exceptions (connecting a third-party’s USB disk to our system to transfer data   from/to it).

USBGuard  can lockdown the system and permit only known devices to create interfaces to it via USB. It is possible to bypass this  protection by  emulating the devices on the whitelist. However, this requires the knowledge of the contents of such list and for devices which export a  unique serial number, it’s even harder as one needs to know this serial  number to successfully emulate the device.

Reprogramming  attacks ([BadUSB](https://srlabs.de/badusb/)) which change the type of the device or add a hidden/unexpected interface to the device (a USB keyboard/network card  interface on a USB flash disk) will change the attributes of the  device and it will no longer match the whitelist.

Allowing to attach any USB device to the system exposes it to exploitation of bugs in USB interface drivers in the kernel.

### Use case #2: USB device blacklisting
The USB protocol uses a classification system for the various types  of  interfaces a USB device can provide. If an user doesn’t want to use a particular class of interfaces, he can block devices that want to communicate with the computer as an interface from that class.

### Use case #3: Triggering actions on USB device events
The project makes possible to easily implement triggering of various actions when a particular USB device or USB device class is inserted, removed, etc. This feature might be used for example for auditing USB usage, screen locking (per-user via applet), etc.

## Supported Operating Systems

Currently, USBGuard works only on Linux. To enforce the user-defined policy, it uses the USB device authorization feature
implemented in the Linux kernel since the year 2007. Read [this document](https://www.kernel.org/doc/Documentation/usb/authorization.txt)
if you want to know more.

### Portability

Altought the primary target and development platform of this project is Linux, the code aims to be portable to other operating systems as well. Internally, the USBGuard daemon tries to abstract USB device handling as much as possible and for this purpose it uses two base classes that define the interface which the daemon uses for interacting with USB devices. A USB device is represented by the `usbguard::Device` class. The seconds class, `usbguard::DeviceManager`, defines an interface for USB device discovery and authorization. Please refer to the `usbguard::LinuxDevice` and `usbguard::LinuxDeviceManager` classes for an example implementation. More detailed documentation will be added as soon as possible.

## Compilation

If you want to compile the sources from a release tarball, you'll have to install development files for:

 * [libqb](https://github.com/ClusterLabs/libqb) - used for IPC
 * [libsodium](http://libsodium.org) - used for hashing
 * systemd-devel - used for udev

Optionally, you may want to install:

 * [libseccomp](https://github.com/seccomp/libseccomp) - used to implement a syscall whitelist
 * [libcap-ng](https://people.redhat.com/sgrubb/libcap-ng/) - used to drop process capabalities

And then do:

    $ ./configure
    $ make
    $ sudo make install

After the sources are successfully built, you can run the testsuite by executing:

    $ make check

If you want to compile the sources in a cloned repository, there are additional step required:

    $ git submodule init
    $ git submodule update

This will fetch the sources of [json](https://github.com/nlohmann/json/), [spdlog](https://github.com/gabime/spdlog) and [Catch](https://github.com/philsquared/Catch) which are used in this project too.

And to generate the *configure* script, run:

    $ ./autogen.sh

If you want to modify the lexer and/or the parser, you'll have to generate new source files for them. To learn how to do that, read [src/Library/RuleParser/README.md](src/Library/RuleParser/README.md).

## Pre-compiled packages

### Fedora Linux, RHEL or CentOS

Pre-compiled packages for Fedora 20, 21, 22, rawhide and EPEL 7 (RHEL, CentOS) are distributes using a Copr [repository](https://copr.fedoraproject.org/coprs/mildew/usbguard/).
You can install the repository by executing the following steps:

    $ sudo yum install yum-plugin-copr
    $ sudo yum copr enable mildew/usbguard
    $ sudo yum install usbguard
    $ sudo yum install usbguard-applet-qt

To actually start the daemon, use:

    $ sudo systemctl start usbguard.service
    $ usbguard-applet-qt

## Rules

The USBGuard daemon decides which USB device to authorize based on a policy defined by a set of rules. When an USB device is inserted into
the system, the daemon scans the existing rules sequentially and when a matching rule is found, it either authorizes (allows), deauthorizes
(blocks) or removes (rejects) the device, based on the rule target. If no matching rule is found, the decision is based on an implicit default
target. This implicit default is to block the device until a decision is made by the user.

The rule language grammar, expressed in a BNF-like syntax, is the following:

    rule ::= target device.

    target ::= "allow" | "block" | "reject".

    device ::= device_id device_attributes.
    device ::= .

    device_id ::= "*:*" | vendor_id ":*" | vendor_id ":" product_id.

    device_attributes ::= device_attributes | attribute.
    device_attributes ::= .

See [Device attributes](https://github.com/dkopecek/usbguard#device-attributes) section for the list of available attributes and their syntax.

### Targets

The target of a rule specifies whether the device will be authorized for use or not. Three types of target are recognized:

 * `allow` - authorize the device
 * `block` - deauthorize the device
 * `reject` - remove the device from the system

### Device specification

Except the target, all the other fields of a rule need not be specified. Such a minimal rule will match any device and
allows the policy creator to write an explicit default target (there's an implicit one too, more on that later).
However, if one want's to narrow the applicability of a rule to a set of devices or one device only, it's possible to
do so with a device id and/or device attributes.

#### Device ID

A USB device ID is the colon delimited pair *vendor\_id:product\_id*. All USB devices have this
ID assigned by the manufacturer and it should uniquely identify a USB product. Both *vendor\_id* and *product\_id* are 16-bit
numbers represented in hexadecimal base.

In the rule, it's possible to use an asterisk character to match either any device ID `*:*` or any product ID from a
specific vendor, e.g. `1234:*`.

#### Device attributes

(Please see [issue #11](https://github.com/dkopecek/usbguard/issues/11) and comment on the changes related to this section)

Device attributes are specific value read from the USB device after it's inserted to the system. Which attributes are
available is defined bellow. Some of the attributes are derived or based on attributes read directly from the device.
The value of an attribute is represented as a double-quoted string.

List of attributes:

 * `hash "[0-9a-f]{32}"`: Match a hash of the device attributes (the hash is computed for every device by USBGuard).
 * `name "device-name"`: Match the USB device name attribute. 
 * `serial "serial-number"`: Match the iSerial USB device attribute.
 * `via-port "port-id"`: Match the USB port through which the device is connected.
 * `via-port [operator] { "port-id" "port-id" ... }`: Match a set of USB ports.
 * `with-interface interface-type`: Match an interface the USB device provides.
 * `with-interface [operator] { interface-type interface-type ... }`: Match a set of interface types against the set of interfaces that the USB device provides.

`operator` is one of:
 * `all-of`: The device attribute set must contain all of the specified values for the rule to match.
 * `one-of`: The device attribute set must contain at least one of the specified values for the rule to match.
 * `none-of`: The device attribute set must not contain any of the specified values for the rule to match.
 * `equals`: The device attribute set must contain exactly the same set of values for the rule to match.
 * `equals-ordered`: The device attribute set must contain exactly the same set of values in the same order for the rule to match.

`port-id` is a platform specific USB port identification. On Linux it's in the form "b-n" where `b` and `n` are unsigned integers (e.g. "1-2", "2-4", ...).

`interface-type` represents a USB interface and should be formated as three 8-bit numbers in hexadecimal base delimited by colon, i.e. `cc:ss:pp`. The numbers represent the interface class (`cc`), subclass (`ss`) and protocol (`pp`) as assigned by the [USB-IF](www.usb.org/about) ([List of assigned classes, subclasses and protocols](http://www.usb.org/developers/defined_class)). Instead of the subclass and protocol number, you may write an asterisk character (`\*`) to match all subclasses or protocols. Matching a specific class and a specific protocol is not allowed, i.e. if you use an asterisk as the subclass number, you have to use an asterisk for the protocol too.

### Initial policy

Using the `usbguard` CLI tool and its `generate-policy` subcommand, you can generate an initial policy for your system instead of writing one from scratch. The tool generates an **allow** policy for all devices connected to the system at the moment of execution. It has several options to tweak the resulting policy:

 * `-p`: Generate port specific rules for all devices. By default, port specific rules are generated only for devices which do not export an iSerial value. See the `-P` option for more details.

 * `-P`: Don't generate port specific rules for devices without an iSerial value. Without this option, the tool will add a `via-port` attribute to any device that doesn't provide a serial number. This is a security measure to limit devices that cannot be uniquely identified to connect only via a specific port. This makes it harder to bypass the policy since the real device will ocupy the allowed USB port most of the time.

 * `-t <target>`: Generate an explicit "catch all" rule with the specified target. The target can be one of the following values: `allow`, `block`, `reject`.

 * `-X`: Don't generate a hash attribute for each device.

 * `-H`: Generate a hash-only policy.

The policy will be printed out on the standard output. It's a good idea to review the generated rules before using them on a system. The typical workflow for generating an initial policy could look like this:

    # usbguard generate-policy > rules.conf
    # vi rules.conf
    (review/modify the rule set)
    # sudo install -m 0600 -o root -g root rules.conf /etc/usbguard/rules.conf
    # sudo systemctl restart usbguard

### Example policies

The following examples show what to put into the `rules.conf` file in order to implement the given policy.

#### Allow USB mass storage devices (USB flash disks) and block everything else

This policy will block any device that isn't just a mass storage device. Devices with a hidden keyboard interface in a USB flash disk will be blocked. Only devices with a single mass storage interface will be allowed to interact with the operating system. The policy consists of a single rule:

    allow with-interface equals { 08:*:* }

The blocking is implicit in this case because we didn't write a `block` rule. Implicit blocking is useful to desktop users because a desktop applet listening to USBGuard events can ask the user for a decision if an implicit target was selected for a device.

#### Allow a specific Yubikey device to be connected via a specific port. Reject everything else on that port.

    allow 1050:0011 name "Yubico Yubikey II" serial "0001234567" via-port "1-2" hash "044b5e168d40ee0245478416caf3d998"
    reject via-port "1-2"

We could use just the hash to match the device. However, using the name and serial attributes allows the policy creator to quickly assign rules to specific devices without computing the hash. On the other hand, the hash is the most specific value we can use to identify a device in USBGuard so it's the best attribute to use if you want a rule to match just one device.

#### Reject devices with suspicious combination of interfaces

A USB flash disk which implements a keyboard or a network interface is very suspicious. The following set of rules forms a policy which allows USB flash disks and explicitly rejects devices with an additional and suspicious (as defined before) interface.

    allow with-interface equals { 08:*:* }
    reject with-interface all-of { 08:*:* 03:00:* }
    reject with-interface all-of { 08:*:* 03:01:* }
    reject with-interface all-of { 08:*:* e0:*:* }
    reject with-interface all-of { 08:*:* 02:*:* }
   
The policy rejects all USB flash disk devices with an interface from the HID/Keyboard, Communications and Wireless classes. Please note that blacklisting is the wrong approach and you shouldn't just blacklist a set of devices and allow the rest. The policy above assumes that blocking is the implicit default. Rejecting a set of devices considered as "bad" is a good approach how to limit the exposure of the OS to such devices as much as possible.
