<?php
    $host = /*DB IP*/
    $user = /*DB ID*/
    $pw = /*DB Password*/
    $dbName = /*DB Name*/

    $conn = new mysqli($host, $user, $pw, $dbName);

    $name=$_POST['userid'];
    $password=$_POST['password'];
    $node_num=$_POST['node'] ;

    if($conn){ echo "Connection established<br>"; }
    else{ die( 'Could not connect: ' . mysqli_error($conn) ); }

    $sql = "INSERT INTO user_info(id, password, node_num, service) VALUES('$name','$password', '$node_num', 0)";
    $result = mysqli_query($conn, $sql);
 	
    if($result) { echo("<script>alert(\"success\");</script>");  }
    else { echo("<script>alert(\"fail\");</script>");  }

    
    echo("<script>location.replace('index.html');</script>");
    mysqli_close($conn);
?>