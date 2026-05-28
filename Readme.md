For the docker-compose.yaml to work, it needs some environment variables.

1) Create a .env file in the root folder. It should be set up like this:

    MYSQL_ROOT_PASSWORD=your_root_password_here
    MYSQL_DATABASE=your_database_name_here
    MYSQL_USER=your_username_here
    MYSQL_PASSWORD=your_password_here
    PORT_MYSQL=3306:3306
    PORT_FRONTEND=3000:3000
    PORT_BACKEND=5678:5678
    TIMEZONE=Europe/Oslo
    RESTART=always

    - The example values for the ports are default for the services, and might already be occupied. Change these if necessary.
    - If you want, you can change the RESTART condition to any of the following to your preference:
        - no
        - always
        - on-failure
        - unless-stopped

2) Run "docker compose up --build" in the root folder.
    - The first time you build the project, it may take a couple of minutes to start.

3) run this in the container to give user access to shared volume:
    - sudo chown -R 1000:1000 /var/lib/docker/volumes/shared_data/_data

4) Add a code node in n8n to send variables via json to c++ backend to reach vue

    const fs = require('fs');

    const data = {
    message: "sun",
    timestamp: new Date().toISOString()
    };

    fs.writeFileSync(
    '/shared/output.json',
    JSON.stringify(data, null, 2)
    );

    return [
    {
        json: {
        success: true,
        timestamp: data.timestamp,
        message: data.message
        }
    }
    ];