# Leon's torso weights, graded up the spine (used by build_leon.py and regrade_leon.py).
# The avatar's torso hangs on two bones (the hips and the chest) and its weights jump from one to the other over a
# hand's breadth, so the takes, which turn Spine3 (nearly all of the twist) and bend Spine1-2, and ClaireOutside's
# upper-body turn, which weighs each vertex by its Spine weights, put the whole twist into 10 cm of the waist: it
# snapped there. The torso's weight (a vertex's share on Hips, Spine1, Spine2 and Spine3 together) is handed up the
# spine by height instead, in smooth steps, as the Limitator's is over 30 cm. A vertex far behind the spine (the
# tail) keeps its own weights.
SPINE=('Hips','Spine1','Spine2','Spine3')
# The takes (Claire's, the Limitator's) turn the twist almost wholly through Spine3 (Spine1 and Spine2 hardly twist,
# they bend), so the share of a vertex on Spine3 is the twist it gets: it must climb slowly up the torso or the whole
# twist sits in a hand's breadth. Leon's joints sit close (the avatar's chest is one bone, Spine1-3 are cut from it), so
# the climb is not tied to them: Spine3's share rises in a smooth step from S3[0] to S3[1], as shares of the way from
# the Hips joint (0) to the Spine3 joint (1), and what is left (the bend) goes to Hips, Spine1 and Spine2 by
# height: all Hips up to LOW[0], Hips to Spine1 up to LOW[1], Spine1 to Spine2 up to LOW[2], all Spine2 above.
S3=(-0.1,1.3); LOW=(-0.15,0.25,0.6)
BEHIND=(0.08,0.18) # behind the Hips joint's y, the torso rule fades out (the tail)

def smooth(t):
    t=max(0.0,min(1.0,t)); return t*t*(3-2*t)

def profile(z,joints):
    """joints: heights of the Hips, Spine1, Spine2 and Spine3 joints. Returns the weights on SPINE at height z."""
    j0,j3=joints[0],joints[3]; at=lambda x:j0+(j3-j0)*x
    w3=smooth((z-at(S3[0]))/(at(S3[1])-at(S3[0])))
    a,b,c=[at(x) for x in LOW]
    if z<=a: lo=[1.0,0.0,0.0]
    elif z<b: s=smooth((z-a)/(b-a)); lo=[1-s,s,0.0]
    elif z<c: s=smooth((z-b)/(c-b)); lo=[0.0,1-s,s]
    else: lo=[0.0,0.0,1.0]
    return [x*(1-w3) for x in lo]+[w3]

def grade(w,co,joints,hips_y):
    """w: {bone name: weight} of one vertex (changed in place); co: its position."""
    m=sum(w.get(n,0.0) for n in SPINE)
    if m<=0: return
    f=1-smooth((co.y-hips_y-BEHIND[0])/(BEHIND[1]-BEHIND[0]))   # 1 on the body, 0 on the tail
    p=profile(co.z,joints)
    for k,n in enumerate(SPINE):
        old=w.get(n,0.0)/m
        x=m*((1-f)*old+f*p[k])
        if x>1e-5: w[n]=x
        else: w.pop(n,None)
