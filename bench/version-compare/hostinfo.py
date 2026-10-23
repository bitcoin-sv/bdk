"""Prints the host's instance type (IMDSv2), CPU and toolchain as JSON."""
import json
import platform
import subprocess
import urllib.request


def sh(cmd):
    try:
        return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10).stdout.strip()
    except Exception as e:
        return str(e)


def imds(path):
    try:
        req = urllib.request.Request("http://169.254.169.254/latest/api/token", method="PUT",
                                     headers={"X-aws-ec2-metadata-token-ttl-seconds": "60"})
        token = urllib.request.urlopen(req, timeout=2).read().decode()
        req = urllib.request.Request("http://169.254.169.254/latest/meta-data/" + path,
                                     headers={"X-aws-ec2-metadata-token": token})
        return urllib.request.urlopen(req, timeout=2).read().decode()
    except Exception:
        return "unknown"


print(json.dumps({
    "instance_type": imds("instance-type"), "arch": platform.machine(), "kernel": platform.release(),
    "go": sh("go version"), "nproc": sh("nproc"),
    "lscpu_model": sh("lscpu | sed -n 's/^ *Model name: *//p' | head -1"),
    "bios_model": sh("lscpu | sed -n 's/^ *BIOS Model name: *//p' | head -1"),
    "cpuinfo_model": sh("sed -n 's/^model name\\s*: //p' /proc/cpuinfo | head -1"),
    "lscpu": sh("lscpu"), "lscpu_e": sh("lscpu -e"),
    "os": sh(". /etc/os-release && echo $PRETTY_NAME"), "gcc": sh("gcc --version | head -1"),
}, indent=1))
