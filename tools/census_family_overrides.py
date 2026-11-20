import struct,os,collections,json,zlib
LO='C:/Modlists/Tabula/profiles/tabula rasa!/plugins.txt'
act=[l.strip()[1:] for l in open(LO,encoding='utf-8',errors='ignore') if l.startswith('*')]
root='C:/Modlists/Tabula/mods'; idx={}
for m in os.listdir(root):
    mp=os.path.join(root,m)
    if os.path.isdir(mp):
        for f in os.listdir(mp):
            if f.lower().endswith(('.esp','.esm','.esl')): idx.setdefault(f.lower(),os.path.join(mp,f))
chain=collections.defaultdict(list)   # key -> [(plugin, kwset)]
kywd={}
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
                if t in (b'ARMO',b'WEAP',b'KYWD'):
                    flags,fid=struct.unpack_from('<II',d,i+8); rb=d[i+24:i+24+sz]
                    if flags&0x40000: rb=zlib.decompress(rb[4:])
                    key=fk(masters,name,fid); kws=set(); edid=None; k=0
                    while k<len(rb):
                        st=rb[k:k+4]; s=struct.unpack_from('<H',rb,k+4)[0]; dat=rb[k+6:k+6+s]
                        if st==b'KWDA': kws={fk(masters,name,struct.unpack_from('<I',dat,q)[0]) for q in range(0,len(dat),4)}
                        elif st==b'EDID': edid=dat.rstrip(b'\0').decode('latin1')
                        k+=6+s
                    if t==b'KYWD': kywd[key]=edid
                    else: chain[(t.decode(),key)].append((name.lower(),frozenset(kws),edid))
                i+=24+sz
    walk(24+size,len(d))
for p in act:
    path=idx.get(p.lower())
    if path: scan(p,path)
waf={k for k,v in kywd.items() if v and v.startswith('WAF_')}
W='weapons armor clothing & clutter fixes.esp'.lower(); C='complete crafting overhaul_remastered.esp'.lower(); A='waccf_armor and clothing extension.esp'.lower()
fam={W,C,A}
touched=0; lost=collections.Counter(); lostkw=collections.Counter(); wins=collections.Counter(); samples=collections.defaultdict(list)
for key,ch in chain.items():
    names=[c[0] for c in ch]
    if W not in names and C not in names and A not in names: continue
    touched+=1
    winner=ch[-1]
    fam_last=max((i for i,c in enumerate(ch) if c[0] in fam))
    fam_rec=ch[fam_last]
    if winner[0] in fam: continue
    wins[winner[0]]+=1
    fw=fam_rec[1]&waf
    if fw and not fw<=winner[1]:
        lostkw[winner[0]]+=1
        if len(samples[winner[0]])<3: samples[winner[0]].append((key[1][0][:3],hex(key[1][1]),fam_rec[2],sorted(kywd[k] for k in (fw-winner[1]))[:3]))
print('ARMO/WEAP records touched by WACCF/ACE/CCOR:',touched)
print('...where a LATER plugin wins:',sum(wins.values()))
print('top winning plugins (any later override):')
for p,n in wins.most_common(14): print(f'  {p[:55]:55s} {n:5d}  (of which WAF keywords lost: {lostkw[p]})')
print('records where the family\'s WAF keywords are LOST in the final winner:',sum(lostkw.values()))
for p,s in list(samples.items())[:6]: print('  e.g.',p[:40],s[:2])
