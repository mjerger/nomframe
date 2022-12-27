import sys, json
from PIL import Image

if len(sys.argv) != 5:
    print("Usage: " + sys.argv[0] + "num_frames ms_per_frame image.png ../data/p/patternname")
    exit(1)

frames  = int(sys.argv[1])
delay   = int(sys.argv[2])
infile  = sys.argv[3]
outfile = sys.argv[4]

print("reading " + infile)

img = Image.open(infile)
img = img.convert("RGB")

data = bytearray()
for y in range(0, img.height):
    for x in range(0, img.width):
        color = img.getpixel((x,y))
        data.append(color[0])
        data.append(color[1])
        data.append(color[2])


print("writing " + outfile + ".pat")
file = open(outfile+".pat", "wb")
file.write(data)
file.close()

print("writing " + outfile + ".json")
jsondata = {"w" : img.width, "h" : int(img.height/frames), "f" : frames, "d" : delay}
file = open(outfile+".json", "w")
file.write(json.dumps(jsondata))
file.close()

print("done")