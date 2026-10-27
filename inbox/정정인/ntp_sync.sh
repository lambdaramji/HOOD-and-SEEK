#!/bin/sh
# 부팅 후 1회 실행: NTP 서버를 IP로 지정해 시각을 맞춘다.
# 보드에 /etc/resolv.conf가 없어 이름 풀이가 안 되므로 IP를 직접 쓴다.
# 설정은 /run(메모리)에만 쓰므로 봉인 이미지를 건드리지 않고, 재부팅하면 사라진다.
#   203.248.240.140 = time.bora.net, 27.96.158.81 = KRISS (2026-09-26 확인)
set -e
mkdir -p /run/systemd/timesyncd.conf.d
printf '[Time]\nNTP=203.248.240.140 27.96.158.81\n' > /run/systemd/timesyncd.conf.d/ntp.conf
systemctl restart systemd-timesyncd
sleep 5
timedatectl timesync-status | grep -E 'Server|Offset|Packet count'
date
