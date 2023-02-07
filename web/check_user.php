<?php
    include 'inc_head.php';

    $host = /*DB IP*/
    $user = /*DB ID*/
    $pw = /*DB Password*/
    $dbName = /*DB Name*/

    $conn = new mysqli($host, $user, $pw, $dbName);

    if($conn){ echo "Connection established"."<br>"; }
    else{ die( 'Could not connect: ' . mysqli_error($conn) ); }

    $username = $_POST['userid'];
    $userpass = $_POST['password'];

    $sql = "SELECT * FROM user_info WHERE id = '$username' AND password = '$userpass'";    

    $result = mysqli_query($conn, $sql);    
    $row = mysqli_fetch_array($result);

    if ($row != null) {
        $_SESSION['username'] = $row['id'];
        $_SESSION['node'] = $row['node_num'];
        $_SESSION['service'] = $row['service'];

        echo("<script>location.replace('index.html');</script>");
        exit;
    }
    else{
        echo("fail");
    }
?>