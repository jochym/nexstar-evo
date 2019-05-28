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
    '1 25544U 98067A   19148.18965350  .00000096  00000-0  94375-5 0  9998',
    '2 25544  51.6436 100.5392 0007107 344.2688 332.3062 15.51154195172148'
)
 
while True:
    home.date = datetime.utcnow()
    iss.compute(home)
    print('iss: altitude %4.2f deg, azimuth %5.2f deg' % (iss.alt * degrees_per_radian, iss.az * degrees_per_radian), end='\r')
    time.sleep(1)

