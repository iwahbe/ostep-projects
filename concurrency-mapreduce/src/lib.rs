#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
// include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_ulong};

#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[no_mangle]
pub extern "C" fn MR_Emit(key: *const c_char, value: *const c_char) {
    unsafe {
        println!(
            "MR_Emit called with key: {:?} and value: {:?}",
            CStr::from_ptr(key),
            CStr::from_ptr(value)
        );
    }
}

#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[no_mangle]
pub extern "C" fn MR_DefaultHashPartition(key: *const c_char, num_partition: c_int) -> u64 {
    unsafe {
        println!(
            "MR_DefaultHashPartition called with key: {:?} and num_partition: {:?}",
            CStr::from_ptr(key),
            num_partition
        );
    }
    0
}

type Mapper = extern "C" fn(*const c_char);
type Reducer = extern "C" fn(*const c_char, *const Getter, c_int);
type Getter = extern "C" fn(*const c_char, c_int) -> *const c_char;
type Partitioner = extern "C" fn(*const c_char, c_int) -> c_ulong;

#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[no_mangle]
pub extern "C" fn MR_Run(
    argc: c_int,
    argv: *const *const c_char,
    map: Mapper,
    num_mappers: c_int,
    reduce: Reducer,
    num_reducers: c_int,
    _partition: Partitioner,
) {
    for i in 0..argc {
        unsafe {
            println!(
                "argv[{}]: {:?}",
                i,
                CStr::from_ptr(((*argv) as usize + i as usize) as *const i8)
            );
        }
    }
    println!(
        "num_mappers, num_reducers: {:?}, {:?}",
        num_mappers as u64, num_reducers as i64
    );
    let file_name = CString::new("my map file_name").unwrap();
    let map_ptr = file_name.as_bytes_with_nul().as_ptr();
    map(map_ptr as *const c_char);
    let reduce_key = CString::new("reduce key").unwrap();
    let reduce_key_ptr = reduce_key.as_bytes_with_nul().as_ptr();
    reduce(
        reduce_key_ptr as *const c_char,
        getter as *const extern "C" fn(*const c_char, c_int) -> *const c_char,
        7,
    );

    println!("MR_RUN called");
}

#[no_mangle]
pub extern "C" fn getter(key: *const c_char, partition_number: c_int) -> *const c_char {
    println!(
        "getter called with key: {:?}, partition_number: {}",
        unsafe { CStr::from_ptr(key) },
        partition_number
    );
    key
}
