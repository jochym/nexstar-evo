import math
import time
from datetime import datetime
import ephem
 
degrees_per_radian = 180.0 / math.pi
 
home = ephem.Observer()
home.lon = '20.033'   # +E
home.lat = '50.083'   # +N
home.elevation = 190 # meters
 
# Always get the latest ISS TLE data from:
# http://spaceflight.nasa.gov/realdata/sightings/SSapplications/Post/JavaSSOP/orbit/ISS/SVPOST.html
iss = ephem.readtle('ISS',
    '1 25544U 98067A   16242.95117257  .00002267  00000-0  41163-4 0  9998',
    '2 25544  51.6453  49.9577 0002889 256.1824 240.1556 15.54345818 16466'
)
 
while True:
    home.date = datetime.utcnow()
    iss.compute(home)
    print('iss: altitude %4.1f deg, azimuth %5.1f deg' % (iss.alt * degrees_per_radian, iss.az * degrees_per_radian))
    time.sleep(10)

