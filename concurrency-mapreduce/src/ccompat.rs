pub mod carray {
    #[no_mangle]
    #[repr(transparent)]
    pub struct CArray<T> {
        head: *const T,
    }

    impl<T> From<*const T> for CArray<T> {
        fn from(head: *const T) -> Self {
            CArray { head }
        }
    }

    impl<T, S: Into<usize>> std::ops::Index<S> for CArray<T> {
        type Output = T;
        fn index(&self, ind: S) -> &Self::Output {
            let u: usize = ind.into();
            unsafe { self.head.offset(u as isize).as_ref().unwrap() }
        }
    }

    impl<T, S: Into<usize>> std::ops::IndexMut<S> for CArray<T> {
        fn index_mut(&mut self, ind: S) -> &mut Self::Output {
            let u: usize = ind.into();
            unsafe { (self.head as *mut T).offset(u as isize).as_mut().unwrap() }
        }
    }

    pub struct Iter<'a, T> {
        array: &'a CArray<T>,
        index: usize,
        max: usize,
    }

    impl<T> CArray<T> {
        pub fn iter_to<'a, S: Into<usize>>(&'a self, len: S) -> Iter<'a, T> {
            Iter {
                array: &self,
                index: 0,
                max: len.into(),
            }
        }
    }
    impl<'a, T> Iterator for Iter<'a, T> {
        type Item = &'a T;
        fn next(&mut self) -> Option<Self::Item> {
            if self.index < self.max {
                let out = &self.array[self.index];
                self.index += 1;
                Some(out)
            } else {
                None
            }
        }
    }
}
