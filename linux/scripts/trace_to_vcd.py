# -*- coding: utf-8 -*-
import argparse
import os
import struct


ALIASES = {
	'tapif_thread': 'IDLE',
	'fake_net': 'IDLE',
	'Tmr Svc': 'IDLE',
}


EXTRA_BYTES = {
	0b00: 0,
	0b01: 1,
	0b10: 3,
	0b11: 7,
}


def parse_header(fp):
	tasks = {}
	while True:
		task_id = fp.read(1)
		task_name_length = fp.read(1)
		if len(task_id) < 1 or len(task_name_length) < 1:
			break
		task_id = struct.unpack('B', task_id)[0]
		task_name_length = struct.unpack('B', task_name_length)[0]
		task_name = fp.read(task_name_length)
		if len(task_name) < task_name_length:
			break
		tasks[task_id] = task_name.decode('ascii')
	return tasks


def parse_trace(fp):
	while True:
		time_diff = fp.read(1)
		if len(time_diff) < 1:
			return
		time_diff = struct.unpack('B', time_diff)[0]
		header = time_diff >> 6
		time_diff = time_diff & 0x3f
		for _ in range(EXTRA_BYTES[header]):
			byte = fp.read(1)
			if len(byte) < 1:
				return
			time_diff = (time_diff << 8) + struct.unpack('B', byte)[0]
		value = fp.read(1)
		if len(value) < 1:
			return
		value = struct.unpack('B', value)[0]
		yield (time_diff, value)



def convert_trace_to_vcd(input_directory, output_filename):
	aliases = {}

	with open(os.path.join(input_directory, 'header'), 'rb') as fp:
		header = parse_header(fp)
	header_idx = {v: k for k, v in header.items()}

	for name, alias in ALIASES.items():
		if name in header_idx and alias in header_idx:
			aliases[header_idx[name]] = header_idx[alias]

	with open(output_filename, 'w') as out_file:
		out_file.write("$date today $end\n")
		out_file.write("$timescale 1 us $end\n")
		out_file.write("$scope module trace $end\n")

		for task, value in sorted(header.items()):
			if task in aliases:
				continue
			name = chr(ord('!') + task)
			value = value.replace(' ', '')
			out_file.write(f"$var wire 1 {name} {value} $end\n")

		out_file.write("$upscope $end\n")
		out_file.write("$enddefinitions $end\n")

		old_task = None
		cumulated_time = 0

		with open(os.path.join(input_directory, 'trace'), 'rb') as fp:
			for time, task in parse_trace(fp):
				cumulated_time += time
				task = aliases.get(task, task)

				new_name = chr(ord('!') + task)
				old_name = None if old_task is None else chr(ord('!') + old_task)
				old_task = task

				if old_name is None:
					out_file.write(f"#{cumulated_time} 1{new_name}\n")
				else:
					out_file.write(f"#{cumulated_time} 0{old_name} 1{new_name}\n")


def main():
	parser = argparse.ArgumentParser(description="Convert trace directory to value change dump format")
	parser.add_argument('directory', type=str, help="Input directory")
	parser.add_argument('file', type=str, help="Output file")
	args = parser.parse_args()
	convert_trace_to_vcd(args.directory, args.file)


if __name__ == "__main__":
	main()
