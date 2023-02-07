<?php
  session_start();
  if( isset( $_SESSION[ 'username' ] ) ) {
    $jb_login = TRUE;
    $username = $_SESSION[ 'username' ];
    if( isset( $_SESSION[ 'service' ] ) ) {
        $service = $_SESSION[ 'service' ];
      }
    
      if( isset( $_SESSION[ 'node' ] ) ) {
        $node = $_SESSION[ 'node' ];
      }
  }
  else {
    $jb_login = FALSE;
  }

  
?>