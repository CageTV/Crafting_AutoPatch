import struct,zlib,collections,sys,os
def scan(path):
    d=open(path,'rb').read(); cnt=collections.Counter(); masters=[]; flags0=0
    def walk(i,end):
        nonlocal flags0
        while i<end:
            t=d[i:i+4]; size=struct.unpack_from('<I',d,i+4)[0]
            if t==b'GRUP': walk(i+24,i+size); i+=size
            else:
                flags,fid=struct.unpack_from('<II',d,i+8); cnt[t.decode()]+=1
                if t==b'TES4':
                    flags0=flags
                    body=d[i+24:i+24+size]; j=0
                    while j<len(body):
                        st=body[j:j+4]; s=struct.unpack_from('<H',body,j+4)[0]
                        if st==b'MAST': masters.append(body[j+6:j+6+s].rstrip(b'\0').decode())
                        j+=6+s
                i+=24+size
    walk(0,len(d)); return cnt,masters,flags0,len(d)
base='C:/Modlists/Tabula/mods/'
for label,rel in (('WACCF','Weapons Armor Clothing and Clutter Fixes/weapons armor clothing & clutter fixes.esp'),
                  ('ACE','Armor and Clothing Extension/waccf_armor and clothing extension.esp'),
                  ('CCOR','Complete Crafting Overhaul Remastered/complete crafting overhaul_remastered.esp')):
    p=base+rel
    if not os.path.exists(p): print(label,'MISSING',p); continue
    c,m,f,n=scan(p)
    print(f'== {label}: {n/1e6:.1f} MB, flags 0x{f:x} (ESM {bool(f&1)}, ESL {bool(f&0x200)}), masters {m}')
    tot=sum(c.values()); print('   records',tot,' top types:',', '.join(f'{k} {v}' for k,v in c.most_common(14)))
