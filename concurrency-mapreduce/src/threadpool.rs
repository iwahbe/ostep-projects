use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc;
use std::sync::Arc;
use std::sync::Mutex;
use std::thread;

/// A que of jobs and workers to execute them.
pub struct ThreadPool {
    workers: Vec<Worker>,
    sender: mpsc::Sender<Message>,
    count: Arc<AtomicUsize>,
}

type Job = Box<dyn FnOnce() + Send + 'static>;

/// A message for a worker.
///
/// The worker either has a new job or, the worker shouold terminate.
enum Message {
    NewJob(Job),
    Terminate,
}

impl ThreadPool {
    /// Create a new ThreadPool.
    ///
    /// The size is the number of threads in the pool.
    ///
    /// Adapted from when I read the rust book.
    pub fn new(size: usize) -> Result<ThreadPool, ()> {
        if size == 0 {
            return Err(());
        }
        let (sender, receiver) = mpsc::channel();

        let receiver = Arc::new(Mutex::new(receiver));

        let mut workers = Vec::with_capacity(size);

        let count = Arc::new(AtomicUsize::new(0));

        for _ in 0..size {
            workers.push(Worker::new(Arc::clone(&receiver), count.clone()));
        }

        Ok(ThreadPool {
            workers,
            sender,
            count,
        })
    }

    /// Runs f on the first available worker.
    pub fn execute<F>(&self, f: F)
    where
        F: FnOnce() + Send + 'static,
    {
        let job = Box::new(f);
        self.count.fetch_add(1, Ordering::SeqCst);

        self.sender.send(Message::NewJob(job)).unwrap();
    }

    pub fn wait(&self, max: usize) {
        while self.count.load(Ordering::SeqCst) > max {}
    }
}

impl Drop for ThreadPool {
    fn drop(&mut self) {
        for _ in &self.workers {
            self.sender.send(Message::Terminate).unwrap();
        }
        for worker in &mut self.workers {
            if let Some(thread) = worker.thread.take() {
                thread.join().unwrap();
            }
        }
    }
}

struct Worker {
    thread: Option<thread::JoinHandle<()>>,
}

impl Worker {
    fn new(receiver: Arc<Mutex<mpsc::Receiver<Message>>>, count: Arc<AtomicUsize>) -> Worker {
        let thread = thread::spawn(move || loop {
            let message = receiver.lock().unwrap().recv().unwrap();

            match message {
                Message::NewJob(job) => {
                    job();
                    count.fetch_sub(1, Ordering::SeqCst);
                }
                Message::Terminate => {
                    break;
                }
            }
        });

        Worker {
            thread: Some(thread),
        }
    }
}
