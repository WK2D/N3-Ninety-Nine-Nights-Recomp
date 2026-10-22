"""Check renderer-side evidence from an isolated --graphics_audit run."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
args = parser.parse_args()
text = args.log.read_text(encoding='utf-8')

def require(condition, reason):
    if not condition:
        raise SystemExit('Graphics audit failed: ' + reason)

require('[error]' not in text.lower(), 'renderer reported an error')
steps = {int(n) for n in re.findall(r'Graphics audit step (\d+) accepted true', text)}
require(steps == set(range(6)), 'not all control sequences completed')
require('Graphics audit requests complete' in text, 'run ended before completion')
outputs = {(int(w), int(h)) for w, h in re.findall(r'Display output size: (\d+)x(\d+) pixels', text)}
require({(1280,720),(1920,1080),(2560,1440),(3840,2160)} <= outputs, 'display pixel dimensions did not match every preset')
sources = {(int(w), int(h)) for w, h in re.findall(r'Graphics swap source: (\d+)x(\d+)', text)}
require({(1280,720),(2560,1440),(3840,2160)} <= sources, 'rendered source dimensions did not match every scale')
probes = {int(n) for n in re.findall(r'Graphics sampler probe: override (-?\d+) accepted true', text)}
require({0,2,3,4,5} <= probes, 'sampler eligibility/override probes did not pass for every choice')
descriptors = {(int(n), int(a)) for n, a in re.findall(r'Graphics sampler: override (-?\d+) filter \d+ actual anisotropy (\d+)', text)}
require({(0,1),(2,2),(3,4),(4,8),(5,16)} <= descriptors, 'actual sampler descriptor limits did not match their labels')
policies = {(sync, int(interval), int(flags)) for sync, interval, flags in re.findall(r'Graphics host present: VSync (true|false) interval (\d+) flags (\d+)', text)}
require(('true',1,8) in policies, 'nonblocking synchronized presentation was not observed')
require(any(sync == 'false' and interval == 0 and flags in {4,516} for sync,interval,flags in policies), 'immediate presentation was not observed')
print('Display sizes, rendered source sizes, all filtering choices, and host VSync policies verified')
print('Video source cadence and these probes do not establish gameplay animation timing or full-game visual coverage')
