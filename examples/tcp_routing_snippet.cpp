String incoming = tcpManager.readLine();

if (incoming.startsWith("LCD:")) {
    String response = displayCmd.handleTcpCommand(incoming);
    tcpManager.sendLine(response);
    return;
}
