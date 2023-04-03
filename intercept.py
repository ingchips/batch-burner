import burner

# return abort_flag, new_data
def on_start_bin(batch_counter: int, bin_index: int, data: bytes):
    if bin_index != 2:
        return False, data
    ba = bytearray(data)
    addr = batch_counter.to_bytes(4, 'little')
    ba[0:4] = addr
    
    burner.AppendLog('[Script]Data Size %d Counter:%d\n' % (len(data), batch_counter))
    
    return False, bytes(ba)
