import struct,os,collections,sys,json
LO='C:/Modlists/Tabula/profiles/tabula rasa!/plugins.txt'
act=[l.strip()[1:] for l in open(LO,encoding='utf-8',errors='ignore') if l.startswith('*')]
idx={}
root='C:/Modlists/Tabula/mods'
for m in os.listdir(root):
    mp=os.path.join(root,m)
    if not os.path.isdir(mp): continue
    for f in os.listdir(mp):
        if f.lower().endswith(('.esp','.esm','.esl')): idx.setdefault(f.lower(),os.path.join(mp,f))
want=(b'ARMO',b'WEAP',b'COBJ',b'AMMO',b'OTFT',b'LVLI')
def scan(path):
    d=open(path,'rb').read(); n=len(d)
    # TES4
    size=struct.unpack_from('<I',d,4)[0]; body=d[24:24+size]; j=0; masters=[]
    while j<len(body):
        st=body[j:j+4]; s=struct.unpack_from('<H',body,j+4)[0]
        if st==b'MAST': masters.append(1)
        j+=6+s
    own=len(masters); newc=collections.Counter(); ovr=collections.Counter()
    def walk(i,end):
        while i<end:
            t=d[i:i+4]; sz=struct.unpack_from('<I',d,i+4)[0]
            if t==b'GRUP': walk(i+24,i+sz); i+=sz
            else:
                if t in want:
                    fid=struct.unpack_from('<I',d,i+12)[0]
                    (newc if (fid>>24)==own else ovr)[t.decode()]+=1
                i+=24+sz
    walk(24+size,n)
    return newc,ovr
rows=[];missing=[]
for p in act:
    path=idx.get(p.lower())
    if not path: missing.append(p); continue
    try: nc,ov=scan(path)
    except Exception as e: missing.append(p+' ERR'); continue
    rows.append((p,nc,ov))
json.dump([(p,dict(n),dict(o)) for p,n,o in rows],open('census2.json','w'))
print('scanned',len(rows),'missing',len(missing),missing[:5])
tot=collections.Counter(); tot_o=collections.Counter()
for p,n,o in rows: tot.update(n); tot_o.update(o)
print('NEW records across the list:',dict(tot)); print('OVERRIDES across the list:',dict(tot_o))
top=sorted(rows,key=lambda r:-(r[1].get('ARMO',0)+r[1].get('WEAP',0)))[:22]
print('plugins adding the most armor+weapons (new):')
for p,n,o in top: print(f"  {p[:58]:58s} ARMO {n.get('ARMO',0):4d} WEAP {n.get('WEAP',0):4d} COBJ {n.get('COBJ',0):4d} | overrides ARMO {o.get('ARMO',0):4d} WEAP {o.get('WEAP',0):4d} COBJ {o.get('COBJ',0):4d}")
