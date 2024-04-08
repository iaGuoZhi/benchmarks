import subprocess
import re

machine_list = {
    "guozhi-ipads": "114.88.179.123"
}

site_list = {
    "github": "www.github.com",
    "google": "www.google.com",
    "notion": "www.notion.so",
    "youtube": "www.youtube.com",
    "twitter": "www.twitter.com",
}

country_list = {
    "us": "www.google.com",
    "cn": "www.baidu.com",
    "hk": "www.google.com.hk",
    "sg": "www.google.com.sg",
    "jp": "www.yahoo.co.jp",
    "uk": "www.bbc.co.uk",
}

def ping_latency(ip):
    # ping a host and return the latency
    try:
        output = subprocess.check_output(["ping", "-c", "1", ip])
        latency = re.search(r"min/avg/max/mdev = \d+\.\d+/(\d+\.\d+)/\d+\.\d+/\d+\.\d+", output.decode("utf-8")).group(1)
        return latency
    except Exception as e:
        return None

if __name__ == "__main__":
    print("Machine latency:")
    for machine, ip in machine_list.items():
        latency = ping_latency(ip)
        if latency:
            print(f"{machine}: {latency} ms")
        else:
            print(f"{machine}: unreachable")
    print("Site latency:")
    for site, ip in site_list.items():
        latency = ping_latency(ip)
        if latency:
            print(f"{site}: {latency} ms")
        else:
            print(f"{site}: unreachable")
    print("Country latency:")
    for country, ip in country_list.items():
        latency = ping_latency(ip)
        if latency:
            print(f"{country}: {latency} ms")
        else:
            print(f"{country}: unreachable")
