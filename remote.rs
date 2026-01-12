use std::io::{Read, Write};
use std::net::TcpStream;
use std::time::Duration;
use std::convert::TryFrom;

const MAGIC: u8 = 0xA5;

#[repr(u8)]
#[derive(Debug, Clone, Copy)]
enum Cmd {
    Ping = 0x01,
    GetStatus = 0x02,
    SetRelay = 0x03,
    ToggleRelay = 0x04,
    SetAll = 0x05,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
enum RespType {
    Ok,
    Err,
    Status,
    Pong,
}

impl TryFrom<u8> for RespType {
    type Error = String;
    
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0x00 => Ok(RespType::Ok),
            0x01 => Ok(RespType::Err),
            0x02 => Ok(RespType::Status),
            0x03 => Ok(RespType::Pong),
            _ => Err(format!("Unknown response type: 0x{:02X}", value)),
        }
    }
}

struct RelayClient {
    addr: String,
    timeout: Duration,
}

impl RelayClient {
    fn new(addr: &str) -> Self {
        Self {
            addr: addr.to_string(),
            timeout: Duration::from_secs(2),
        }
    }
    
    fn send_command(&self, cmd: Cmd, relay_id: u8, value: u8) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let mut stream = TcpStream::connect(&self.addr)?;
        stream.set_read_timeout(Some(self.timeout))?;
        stream.set_write_timeout(Some(self.timeout))?;
        
        // Send request
        let packet = [MAGIC, cmd as u8, relay_id, value];
        stream.write_all(&packet)?;
        
        // Read response
        let mut response = vec![0u8; 64];
        let n = stream.read(&mut response)?;
        response.truncate(n);
        
        // Validate magic byte
        if response.is_empty() || response[0] != MAGIC {
            return Err("Invalid response magic byte".into());
        }
        
        Ok(response)
    }
    
    fn ping(&self) -> Result<(), Box<dyn std::error::Error>> {
        let resp = self.send_command(Cmd::Ping, 0, 0)?;
        
        if resp.len() >= 2 {
            let resp_type = RespType::try_from(resp[1])?;
            if resp_type == RespType::Pong {
                return Ok(());
            }
        }
        
        Err("Invalid ping response".into())
    }
    
    fn get_status(&self) -> Result<u8, Box<dyn std::error::Error>> {
        let resp = self.send_command(Cmd::GetStatus, 0, 0)?;
        
        if resp.len() >= 4 {
            let resp_type = RespType::try_from(resp[1])?;
            if resp_type == RespType::Status {
                return Ok(resp[3]);
            }
        }
        
        Err("Invalid status response".into())
    }
    
    fn set_relay(&self, relay_id: u8, state: bool) -> Result<(), Box<dyn std::error::Error>> {
        let resp = self.send_command(Cmd::SetRelay, relay_id, state as u8)?;
        
        if resp.len() >= 2 {
            let resp_type = RespType::try_from(resp[1])?;
            if resp_type == RespType::Ok {
                return Ok(());
            } else if resp_type == RespType::Err {
                let error_code = if resp.len() >= 4 { resp[3] } else { 0xFF };
                return Err(format!("Device error: 0x{:02X}", error_code).into());
            }
        }
        
        Err("Invalid response".into())
    }
    
    fn toggle_relay(&self, relay_id: u8) -> Result<(), Box<dyn std::error::Error>> {
        let resp = self.send_command(Cmd::ToggleRelay, relay_id, 0)?;
        
        if resp.len() >= 2 {
            let resp_type = RespType::try_from(resp[1])?;
            if resp_type == RespType::Ok {
                return Ok(());
            } else if resp_type == RespType::Err {
                let error_code = if resp.len() >= 4 { resp[3] } else { 0xFF };
                return Err(format!("Device error: 0x{:02X}", error_code).into());
            }
        }
        
        Err("Invalid response".into())
    }
    
    fn set_all(&self, bitmask: u8) -> Result<(), Box<dyn std::error::Error>> {
        let resp = self.send_command(Cmd::SetAll, bitmask, 0)?;
        
        if resp.len() >= 2 {
            let resp_type = RespType::try_from(resp[1])?;
            if resp_type == RespType::Ok {
                return Ok(());
            }
        }
        
        Err("Invalid response".into())
    }
}

fn print_relay_status(status: u8, num_relays: usize) {
    println!("Relay Status:");
    for i in 0..num_relays {
        let state = if (status >> i) & 1 == 1 { "ON" } else { "OFF" };
        println!("  Relay {}: {}", i, state);
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().collect();
    
    if args.len() < 3 {
        eprintln!("Usage: {} <host:port> <command> [args...]", args[0]);
        eprintln!("Commands:");
        eprintln!("  ping");
        eprintln!("  status");
        eprintln!("  set <relay_id> <0|1>");
        eprintln!("  toggle <relay_id>");
        eprintln!("  all <bitmask>");
        return Ok(());
    }
    
    let client = RelayClient::new(&args[1]);
    
    match args[2].as_str() {
        "ping" => {
            client.ping()?;
            println!("Pong!");
        }
        "status" => {
            let status = client.get_status()?;
            print_relay_status(status, 8);
        }
        "set" if args.len() >= 5 => {
            let relay_id: u8 = args[3].parse()?;
            let state: u8 = args[4].parse()?;
            client.set_relay(relay_id, state != 0)?;
            println!("OK");
        }
        "toggle" if args.len() >= 4 => {
            let relay_id: u8 = args[3].parse()?;
            client.toggle_relay(relay_id)?;
            println!("OK");
        }
        "all" if args.len() >= 4 => {
            let bitmask: u8 = u8::from_str_radix(&args[3], 16)?;
            client.set_all(bitmask)?;
            println!("OK");
        }
        _ => {
            eprintln!("Invalid command");
        }
    }
    
    Ok(())
}