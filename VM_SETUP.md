# VM Setup Guide

**If you are already on a native Linux machine, skip this guide entirely.** Clone the repo, install dependencies, and run `scripts/run_demo.sh`. Otherwise you need to setup a linux environment.

The demo also starts an HTTP server on port 8080 inside that environment. Because the server runs on a separate machine (the VM), you open your host machine's browser and point it at the VM's IP address - the VM is the server, your host is the client.

---

## Local VM Setup (VirtualBox)

These instructions apply to any host OS (macOS, Windows, Linux).

### 1. Download the Ubuntu Server ISO

Download **Ubuntu Server 24.04 LTS** for your specific processor architecture.

### 2. Create the VM

Open VirtualBox and crate a new machine with the downloaded image.

**Recommended Hardware:**
- **RAM: 4096 MB**
- **CPUs: 2**
- **Disk: 20 GB**

### 3. Configure Networking

Before starting the VM: **Settings → Network → Adapter 1 → change "Attached to" from `NAT` to `Bridged Adapter`**.

Select your active network interface from the dropdown

**Why Bridged and not NAT:** NAT lets the VM reach the internet but host cannot reach the VM. Bridged puts the VM on the same local network as host, giving it its own IP address. Your browser needs to reach `<vm-ip>:8080` directly, which only works with Bridged networking.

### 4. Install Ubuntu

Boot the VM and walk through the installer, also install SSH.

### 5. Find the VM's IP Address

Log into the VM, then run:

```bash
ip addr show
```

Look for a line like `inet 192.168.x.x/24`, that is the VM's IP address.

From this point on, you can use your host terminal to SSH in - much more comfortable.

```bash
ssh <your-username>@<vm-ip>
```

### 6. Install Dependencies

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential linux-headers-$(uname -r) g++ git
```

---

## Running the Project

```bash
git clone https://github.com/elads888/RoCEv2-congestion-telemetry-engine.git
cd RoCEv2-congestion-telemetry-engine
chmod +x scripts/run_demo.sh
```

Run in normal mode:

```bash
sudo bash scripts/run_demo.sh
```

Run in slow mode:

```bash
sudo ROCEV2_SLOW_MODE=1 bash scripts/run_demo.sh
```

Then open your browser on host at `http://<vm-ip>:8080`.
