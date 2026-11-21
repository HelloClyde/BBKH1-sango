"""Test the compiled H1 portrait loader with modeled firmware file services."""
import hashlib
import json
import struct

import test_mips as base


def main():
    root = base.ROOT
    machine = base.H1Machine(base.ELF)
    dist = root/'dist/h1'
    pack = (dist/'sango.h1p').read_bytes()
    _, side, per_scenario, scenarios, unique = struct.unpack_from('<8s4H',pack)
    assert (side,per_scenario,scenarios)==(72,200,4)
    index = struct.unpack_from('<800H',pack,16)
    frame_bytes = side*side*2
    start = 16+len(index)*2
    files = {b'sango.h1r':(dist/'sango.h1r').read_bytes(),b'sango.h1p':pack}
    handles = {}
    reads = []

    def attach(table,offset,name,parameters,callback):
        address = 0x80001000+len(services)*16
        services.append(name)
        machine.word(table+offset,address)
        machine.write(address,bytes.fromhex('0800e00300000000'))
        machine.symbols[name] = address
        machine.intercept(name,parameters,callback)

    def open_file(args):
        path = machine.text(args[0])
        directory = 'A:\\应用\\数据\\游戏\\'.encode('gbk')
        assert path.startswith(directory) and machine.text(args[1])==b'rb'
        name = path[len(directory):]
        if name not in files:
            return 0
        handle = 0x816f0000+16*len(handles)
        handles[handle] = (name,0)
        return handle

    def close_file(args):
        del handles[args[0]]
        return 0

    def read_file(args):
        destination,size,count,handle = args
        name,offset = handles[handle]
        payload = files[name][offset:offset+size*count]
        machine.write(destination,payload)
        handles[handle] = (name,offset+len(payload))
        reads.append((name,offset,len(payload)))
        return len(payload)//size

    def seek(args):
        handle,offset,origin = args
        name,pos = handles[handle]
        pos = (0,pos,len(files[name]))[origin]+offset
        handles[handle] = (name,pos)
        return pos

    services = []
    machine.word(0x83c00008,0x80800000)
    machine.word(0x83c00010,0x80801000)
    attach(0x80800000,0,'portrait_open',2,open_file)
    attach(0x80800000,4,'portrait_close',1,close_file)
    attach(0x80800000,8,'portrait_read',4,read_file)
    attach(0x80800000,16,'portrait_seek',3,seek)
    attach(0x80800000,20,'portrait_tell',1,lambda args: handles[args[0]][1])
    attach(0x80801000,8,'art_alloc',1,lambda _: 0x81900000)
    attach(0x80801000,12,'art_free',1,lambda _: 0)

    def load_art():
        assert machine.call('h1_ui_assets_load',instruction_limit=500_000_000)==1

    load_art()
    output = machine.symbols['h1_native_pixels']
    for scenario,who in ((1,31),(2,31),(3,92),(4,199)):
        wanted = start+frame_bytes*index[(scenario-1)*200+who]
        assert machine.call('character_portrait',scenario,who,0,0,side)==1
        actual = b''.join(machine.read(output+row*480*2,side*2) for row in range(side))
        assert actual==pack[wanted:wanted+frame_bytes]
        reads_before = len(reads)
        assert machine.call('character_portrait',scenario,who,0,0,44)==1
        assert len(reads)==reads_before, 'repeated portraits should use the cached frame'
    assert all(size in (16,1600,frame_bytes) for name,_,size in reads if name==b'sango.h1p')
    machine.call('h1_ui_assets_free')
    assert not handles

    for broken in (pack[:-2],pack[:16]+struct.pack('<H',unique)+pack[18:],None):
        if broken is None:
            del files[b'sango.h1p']
        else:
            files[b'sango.h1p'] = broken
        load_art()
        assert machine.call('character_portrait',1,31,0,0,side)==0
        assert machine.call('character_portrait',1,13,0,0,side)==1, 'the existing ruler atlas remains usable'
        machine.call('h1_ui_assets_free')
        assert not handles

    result = {'result':'passed','portrait_slots':len(index),'unique_portraits':unique,
              'checks':['four scenario lookups','pixel-exact 72x72 rendering','one-frame streaming and cache',
                        'reject truncated and out-of-range packs','older installs can use portrait fallback'],
              'bda_sha256':hashlib.sha256((dist/'H1Sango.bda').read_bytes()).hexdigest(),
              'portrait_sha256':hashlib.sha256(pack).hexdigest()}
    print(json.dumps(result))


if __name__=='__main__':
    main()
