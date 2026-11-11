# Generates Dino's 20x20 sprite frames (facing right) as C string arrays.
from PIL import Image
N=20
def tri(g,pts,c):
    (x0,y0),(x1,y1),(x2,y2)=pts
    def s(ax,ay,bx,by,px,py): return (px-bx)*(ay-by)-(ax-bx)*(py-by)
    for y in range(N):
        for x in range(N):
            px,py=x+.5,y+.5
            d1=s(px,py,x0,y0,x1,y1);d2=s(px,py,x1,y1,x2,y2);d3=s(px,py,x2,y2,x0,y0)
            if not ((d1<0 or d2<0 or d3<0) and (d1>0 or d2>0 or d3>0)): g[y][x]=c
LEGS={'idle':[(5,13,4,5),(11,13,4,5)],'blink':[(5,13,4,5),(11,13,4,5)],
      'run1':[(3,13,4,5),(12,12,4,5)],'run2':[(6,13,4,5),(10,13,4,5)],'run3':[(4,12,4,5),(11,13,4,5)],
      'jump':[(5,12,4,4),(11,11,4,4)],'fall':[(5,14,4,5),(11,14,4,5)],
      'hurt':[(3,13,4,5),(13,13,4,5)],'attack':[(2,13,4,5),(8,13,4,5)],'wall':[(5,13,4,5),(11,12,4,5)]}
def frame(kind):
    g=[['.']*N for _ in range(N)]
    b=1 if kind in('run2',) else 0
    lean=2 if kind=='attack' else 0
    cx,cy,rx0,ry=9.0+lean,8.4+b,4.9,6.0
    # legs first so the body sits on top of them
    for (lx,ly,lw,lh) in LEGS[kind]:
        for y in range(ly+b,min(N-1,ly+lh+b)):
            for x in range(lx,lx+lw): g[y][x]='w'
    tri(g,[(0.6+lean,0.0+b),(10.5+lean,3.0+b),(5.0+lean,8.6+b)],'g')
    tri(g,[(0.0+lean,10.6+b),(5.6+lean,7.4+b),(5.0+lean,14.0+b)],'g')
    for y in range(N):
        for x in range(N):
            px,py=x+.5,y+.5
            t=(py-cy)/ry
            rx=rx0+0.5*max(0,t)
            if t*t+((px-cx)/rx)**2<=1.0: g[y][x]='w'
    filled={(x,y) for y in range(N) for x in range(N) if g[y][x]!='.'}
    for (x,y) in list(filled):
        for dx,dy in((1,0),(-1,0),(0,1),(0,-1)):
            a,c=x+dx,y+dy
            if 0<=a<N and 0<=c<N and (a,c) not in filled: g[c][a]='k'
    # a line where the legs leave the body, and where spikes meet the body
    body=lambda x,y:((y+.5-cy)/ry)**2+((x+.5-cx)/(rx0+0.5*max(0,(y+.5-cy)/ry)))**2<=1.0
    for y in range(N):
        for x in range(N):
            if g[y][x]=='w' and not body(x,y) and y>0 and body(x,y-1) and y>=11:
                g[y-1][x]='w'
            if g[y][x]=='g' and any(0<=x+dx<N and 0<=y+dy<N and g[y+dy][x+dx]=='w' for dx,dy in((1,0),(0,1))):
                g[y][x]='k'
    # leg separation
    if kind not in('jump',):
        l0,l1=LEGS[kind]
        gx=(l0[0]+l0[2]+l1[0])//2
        bot=max(l0[1]+l0[3],l1[1]+l1[3])+b
        for y in range(15+b,min(N,bot)):
            if g[y][gx]=='w': g[y][gx]='k'
    ex=10+lean; ey=4+b
    if kind=='blink':
        g[ey+3][ex]='k'; g[ey+3][ex+2]='k'
    elif kind=='hurt':
        for (x,y) in [(ex-1,ey+1),(ex+1,ey+1),(ex,ey+2),(ex-1,ey+3),(ex+1,ey+3)]: g[y][x]='k'
        for (x,y) in [(ex+2,ey+1),(ex+4,ey+1),(ex+3,ey+2),(ex+2,ey+3),(ex+4,ey+3)]: g[y][x]='k'
    elif kind=='attack':
        for y in range(ey+1,ey+4): g[y][ex]='k'; g[y][ex+2]='k'
        g[ey][ex-1]='k'; g[ey][ex+1]='k'   # angry brows
    else:
        for y in range(ey,ey+5): g[y][ex]='k'; g[y][ex+2]='k'
    my=ey+6
    if kind=='hurt':
        for x in range(ex,ex+3): g[my][x]='k'
    elif kind=='attack':
        for x in range(ex-1,ex+4): g[my][x]='k'
        g[my+1][ex]='k'; g[my+1][ex+2]='k'
    else:
        g[my][ex-1]='k';g[my][ex+1]='k';g[my][ex+3]='k';g[my+1][ex]='k';g[my+1][ex+2]='k'
    return [''.join(r) for r in g]
pal={'.':(0,0,0,0),'k':(20,20,20,255),'w':(255,255,255,255),'g':(76,140,43,255)}
kinds=['idle','blink','run1','run2','run3','jump','fall','hurt','attack','wall']
out=open('dino_sprites.h','w')
out.write('// Dino, 20x20, facing right. Made by tools/dino_gen.py.\n')
im=Image.new('RGBA',((N+2)*len(kinds),N),(120,190,240,255))
for i,k in enumerate(kinds):
    f=frame(k)
    out.write('static const char* SPR_DINO_%s[20] = {\n'%k.upper()+''.join('  "%s",\n'%r for r in f)+'};\n')
    for y,r in enumerate(f):
        for x,c in enumerate(r):
            if c!='.': im.putpixel((i*(N+2)+x,y),pal[c])
im.resize((im.width*8,im.height*8),Image.NEAREST).save('preview.png')
