#!/usr/bin/env python

from mininet.topo import Topo
from mininet.util import dumpNodeConnections
from mininet.net import Mininet
from mininet.node import OVSController
from mininet.log import setLogLevel
from subprocess import call

class ThreeLayerTopo(Topo):
    def __init__(self):
        super(ThreeLayerTopo, self).__init__()

        # create switches and add them to lists for reference
        # note: switch/host number is list index offset by 1
        core = self.addSwitch('c1')
        aggregations = [
            self.addSwitch('a1'),
            self.addSwitch('a2')
        ]
        edges = [
            self.addSwitch('e1'),
            self.addSwitch('e2'),
            self.addSwitch('e3'),
            self.addSwitch('e4')
        ]
        hosts = []
        for i in range(1,9):
            hosts.append(self.addHost('h' + str(i)))

        # create links between edge switches and hosts
        for i in range(0,4):
            self.addLink(edges[i], hosts[2*i])
            self.addLink(edges[i], hosts[2*i+1])

        # create links between aggregation switches and edge switches
        for i in range(0,2):
            self.addLink(aggregations[i], edges[2*i])
            self.addLink(aggregations[i], edges[2*i+1])

        # create links between core switch and aggregation switches
        self.addLink(core, aggregations[0])
        self.addLink(core, aggregations[1])


def speed2int(s):
    scale = {
        'gbits/sec': 1e9,
        'mbits/sec': 1e6,
        'kbits/sec': 1e3,
        'bits/sec': 1
    }
    l = s.split(' ')
    return int(float(l[0]) * scale[l[1].lower()])

def speed2str(n):
    if n >= 1e9:
        return str(float(n) / 1e9) + ' Gbps'
    if n >= 1e6:
        return str(float(n) / 1e6) + ' Mbps'
    if n >= 1e3:
        return str(float(n) / 1e3) + ' Kbps'
    return str(n) + ' bps'


# stop existing OVS controllers
call(["pkill", "ovs-controller"])
# verbose output for mininet
setLogLevel('info')
topo = ThreeLayerTopo()
net = Mininet(topo=topo, controller=OVSController)
net.start()

print "Node connections dump:"
dumpNodeConnections(net.hosts)
print "Connectivity (ping) test:"
net.pingAll()

def test_speed(host1_no, host2_no):
    server_speed, client_speed = net.iperf((
        net.get('h' + str(host1_no)),
        net.get('h' + str(host2_no))
    ))
    return speed2int(server_speed)

print "##### Throughput in same rack:"
speeds = [
    test_speed(1,2),
    test_speed(3,4),
    test_speed(5,6),
    test_speed(7,8)
]
print "##### Average throughput: " + speed2str(sum(speeds) / len(speeds))
print ""

print "##### Throughput in different racks but in same aggregation switch:"
speeds = [
    test_speed(1,3),
    test_speed(2,4),
    test_speed(5,7),
    test_speed(6,8)
]
print "##### Average throughput: " + speed2str(sum(speeds) / len(speeds))
print ""

print "##### Throughput in different racks and different aggregation switches:"
speeds = [
    test_speed(1,5),
    test_speed(2,6),
    test_speed(3,7),
    test_speed(4,8)
]
print "##### Average throughput: " + speed2str(sum(speeds) / len(speeds))
print ""

net.stop()
