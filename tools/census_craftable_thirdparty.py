import struct,os,collections,json,zlib
LO='C:/Modlists/Tabula/profiles/tabula rasa!/plugins.txt'
act=[l.strip()[1:] for l in open(LO,encoding='utf-8',errors='ignore') if l.startswith('*')]
root='C:/Modlists/Tabula/mods'; idx={}
for m in os.listdir(root):
    mp=os.path.join(root,m)
    if os.path.isdir(mp):
        for f in os.listdir(mp):
            if f.lower().endswith(('.esp','.esm','.esl')): idx.setdefault(f.lower(),os.path.join(mp,f))
items={}   # (plugin,local)->dict(type,edid,kw[(plugin,local)])
cobj=[]    # (plugin, created(plugin,local), bench(plugin,local))
kywd={}    # (plugin,local)->edid
def fk(masters,self_name,fid):
    i=fid>>24; return ((masters[i] if i<len(masters) else self_name).lower(), fid&0xFFFFFF)
def scan(name,path):
    d=open(path,'rb').read(); size=struct.unpack_from('<I',d,4)[0]; body=d[24:24+size]; j=0; masters=[]
    while j<len(body):
        st=body[j:j+4]; s=struct.unpack_from('<H',body,j+4)[0]
        if st==b'MAST': masters.append(body[j+6:j+6+s].rstrip(b'\0').decode())
        j+=6+s
    def walk(i,end):
        while i<end:
            t=d[i:i+4]; sz=struct.unpack_from('<I',d,i+4)[0]
            if t==b'GRUP': walk(i+24,i+sz); i+=sz
            else:
                if t in (b'ARMO',b'WEAP',b'COBJ',b'KYWD'):
                    flags,fid=struct.unpack_from('<II',d,i+8)
                    rb=d[i+24:i+24+sz]
                    if flags&0x40000: rb=zlib.decompress(rb[4:])
                    key=fk(masters,name,fid); edid=None; kws=[]; created=None; bench=None; fl=flags
                    k=0
                    while k<len(rb):
                        st=rb[k:k+4]; s=struct.unpack_from('<H',rb,k+4)[0]; dat=rb[k+6:k+6+s]
                        if st==b'XXXX': pass
                        if st==b'EDID': edid=dat.rstrip(b'\0').decode('latin1')
                        elif st==b'KWDA': kws=[fk(masters,name,struct.unpack_from('<I',dat,q)[0]) for q in range(0,len(dat),4)]
                        elif st==b'CNAM' and t==b'COBJ': created=fk(masters,name,struct.unpack_from('<I',dat,0)[0])
                        elif st==b'BNAM' and t==b'COBJ': bench=fk(masters,name,struct.unpack_from('<I',dat,0)[0])
                        k+=6+s
                    if t in (b'ARMO',b'WEAP'): items.setdefault(key,dict(type=t.decode(),edid=edid,kw=set(),defined=name.lower(),over=0))['kw'].update(kws)
                    elif t==b'KYWD': kywd[key]=edid
                    else: cobj.append((name,key,created,bench,edid))
                i+=24+sz
    walk(24+size,len(d))
for p in act:
    path=idx.get(p.lower())
    if path: scan(p,path)
print('items',len(items),'keywords',len(kywd),'cobj',len(cobj))
waf={k for k,v in kywd.items() if v and v.startswith('WAF_')}
treat={k for k,v in kywd.items() if v and (v.startswith('WAF_TreatAsMaterial') or v.startswith('WAF_Material') or 'ArmorMaterial' in v or 'WeapMaterial' in v)}
print('WAF keywords',len(waf),'material-ish keywords',len(treat))
# craftable items
made=collections.defaultdict(set)
for pl,key,created,bench,edid in cobj:
    if created in items: made[created].add(pl.lower())
byplugin=collections.defaultdict(lambda:[0,0,0])
for key,pls in made.items():
    it=items[key]; plug=key[0]
    byplugin[plug][0]+=1
    if it['kw']&waf: byplugin[plug][1]+=1
    if it['kw']&treat: byplugin[plug][2]+=1
vanilla={'skyrim.esm','update.esm','dawnguard.esm','hearthfires.esm','dragonborn.esm'}
tp=[(p,v) for p,v in byplugin.items() if p not in vanilla]
tot=sum(v[0] for p,v in tp)
print('craftable third-party items (created by some COBJ):',tot,'in',len(tp),'plugins')
print('plugin'.ljust(50),'craftable  w/WAF kw  w/material kw')
for p,v in sorted(tp,key=lambda x:-x[1][0])[:30]: print(f'{p[:50]:50s} {v[0]:8d} {v[1]:9d} {v[2]:11d}')
json.dump({p:v for p,v in byplugin.items()},open('census3.json','w'))
