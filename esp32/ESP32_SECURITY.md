# ESP32 Security Considerations for NUT

## Overview

This document outlines security considerations and best practices for deploying NUT (Network UPS Tools) on ESP32 platforms.

## Critical Security Issues

### 1. Default Credentials

**⚠️ CRITICAL: Change all default credentials before production deployment!**

The ESP32 port of NUT includes several hardcoded default credentials that **MUST** be changed:

#### UPS Daemon Credentials
- **Location**: `/usr/local/etc/nut/upsd.users`
- **Default passwords**:
  - User `nut`: password `espdonut`
  - User `monuser`: password `pass`
- **Actions Required**:
  1. Edit `/usr/local/etc/nut/upsd.users` after first boot
  2. Use strong, unique passwords (minimum 12 characters)
  3. Restrict access to configuration files

#### WiFi Credentials
- **Location**: `esp32/main/wifi.c` (compile-time)
- **Default credentials**:
  - SSID: `nut`
  - Password: `espdonut`
- **Actions Required**:
  1. Modify `EXAMPLE_ESP_WIFI_SSID` and `EXAMPLE_ESP_WIFI_PASS` before compilation
  2. Use WPA3 authentication when available
  3. Consider implementing WiFi provisioning (BLE, WPS, or web interface)

### 2. File Permissions

The current implementation uses relaxed file permissions that have been improved but should still be reviewed:

- Configuration directories: `0755` (owner: rwx, group/others: rx)
- Configuration files: Should be set to `0600` or `0640` for sensitive files

**Recommended Actions**:
1. Review and restrict permissions on all configuration files
2. Ensure sensitive files are not world-readable
3. Implement proper user/group ownership when possible

### 3. Network Security

#### Default Network Configuration
- **WiFi AP Mode**: SoftAP uses WPA2-PSK with the compiled-in password (`espdonut` unless changed)
- **UPS Server**: Listens on `0.0.0.0:3493` (all interfaces)

**Recommended Actions**:
1. Use WPA2/WPA3 encryption for WiFi AP mode
2. Implement network access controls
3. Consider using TLS/SSL for UPS server connections
4. Restrict `LISTEN` directive to specific interfaces if possible

## Security Hardening Checklist

### Before Deployment

- [ ] Change all default passwords
  - [ ] UPS daemon users
  - [ ] WiFi credentials
- [ ] Review and restrict file permissions
- [ ] Enable WPA2/WPA3 WiFi encryption
- [ ] Configure network access controls
- [ ] Review and limit exposed services
- [ ] Test security configuration

### Runtime Security

- [ ] Monitor authentication logs
- [ ] Regularly update firmware
- [ ] Audit configuration changes
- [ ] Implement rate limiting for authentication attempts
- [ ] Use secure channels for remote access

### Network Configuration

- [ ] Use strong WiFi encryption (WPA3 preferred)
- [ ] Implement MAC address filtering if needed
- [ ] Use VLANs to isolate UPS management traffic
- [ ] Configure firewall rules
- [ ] Disable unused network services

## Secure Configuration Examples

### Example: Secure upsd.users Configuration

```ini
# /usr/local/etc/nut/upsd.users
[admin]
  password = <USE_STRONG_PASSWORD_HERE>
  actions = SET
  instcmds = ALL

[monitor]
  password = <USE_DIFFERENT_STRONG_PASSWORD>
  upsmon primary
```

### Example: Restricted upsd.conf

```ini
# /usr/local/etc/nut/upsd.conf
LISTEN 192.168.1.100 3493  # Listen only on specific interface
MAXCONN 4
MAXAGE 15
```

## Known Limitations

### Platform Stubs

The ESP32 port includes stub implementations for several POSIX security functions:

- `sigaction()`, `signal()` - Signal handling not implemented
- `fchmod()`, `fchown()`, `chown()` - Permission changes are no-ops
- `setuid()`, `setgid()`, `seteuid()` - Privilege changes are no-ops
- `chroot()` - Chroot is not implemented

**Implication**: Traditional UNIX security mechanisms are not available. Implement alternative security controls at the network and application level.

### Resource Constraints

ESP32 devices have limited resources:
- RAM: ~520KB available
- Flash: Varies by module
- CPU: Dual-core 240MHz

**Recommendations**:
- Monitor memory usage
- Limit number of concurrent connections
- Implement watchdog timers
- Use appropriate task priorities

## Secure Development Practices

### Code Review
- Review all ESP32-specific code for security issues
- Use static analysis tools (cppcheck, clang-tidy)
- Conduct security audits before major releases

### Testing
- Test authentication mechanisms
- Verify access controls
- Test resource exhaustion scenarios
- Validate input handling

### Updates
- Implement secure firmware update mechanism
- Sign firmware images
- Verify signatures before updates
- Maintain rollback capability

## Incident Response

### If Compromise is Suspected

1. **Immediate Actions**:
   - Disconnect device from network
   - Document observed behavior
   - Preserve logs if available

2. **Investigation**:
   - Review authentication logs
   - Check for configuration changes
   - Analyze network traffic

3. **Recovery**:
   - Change all credentials
   - Update firmware
   - Review and harden configuration
   - Reconnect to network with monitoring

## Additional Resources

- [ESP32 Security Features](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/index.html)
- [NUT Security Documentation](https://networkupstools.org/docs/user-manual.chunked/ar01s06.html)
- [WiFi Security Best Practices](https://www.wi-fi.org/discover-wi-fi/security)

## Reporting Security Issues

If you discover a security vulnerability in the ESP32 port of NUT:

1. **Do not** open a public issue
2. Email the security contact (see SECURITY.md in project root)
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if available)

## License and Disclaimer

This security documentation is provided as-is without warranty. Users are responsible for properly securing their deployments according to their specific requirements and threat models.
