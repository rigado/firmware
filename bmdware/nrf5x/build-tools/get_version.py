import sys

with open(sys.argv[1] + "\\version.h") as f:
	version = "_"
	for line in f:
		if "#define FIRMWARE_MAJOR_VERSION" in line:
			version += ''.join(c for c in line if c.isdigit())
		elif "#define FIRMWARE_MINOR_VERSION" in line:
			version += "_"
			version += ''.join(c for c in line if c.isdigit())
		elif "#define FIRMWARE_BUILD_NUMBER" in line:
			version += "_"
			version += ''.join(c for c in line if c.isdigit())
	
	print(version, end="")
	